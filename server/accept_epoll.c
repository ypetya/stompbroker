
#include "../logger.h"

#include <fcntl.h>
#include <errno.h>
// For "select" api
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../lib/constants.h"
#include "../lib/thread_safe_queue.h"
#include "../lib/clone_str.h"
#include "../parse_args.h"
#include "process.h"
#include "../websocket/filter/dataframe.h"
#include "../stomp/data_wrappers/pub_sub.h"
#include "data/cleanup.h"

#include <string.h> // memcpy


void *get_inbound_address(struct sockaddr *sa);

void setnonblocking(int conn_sock);
void do_use_fd(int fd, char * readbuffer, ts_queue * input_queue);

stomp_app_config * config;

void accept_epoll(stomp_app_config * app_config, int listen_sock) {
    config = app_config;

    char read_buffer[config->input_buffer_size + 1];
    read_buffer[config->input_buffer_size] = '\0';
    ws_init_buffer();
    ts_queue * input_queue = process_start_threads();


#define MAX_EVENTS 10
    struct epoll_event ev, events[MAX_EVENTS];
    int conn_sock, nfds, epollfd;
    struct sockaddr_in addr; // connector's address information
    socklen_t addrlen = sizeof addr;
    char * clientAddressStr; //[INET6_ADDRSTRLEN];

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listen_sock) {
                conn_sock = accept(listen_sock,
                        (struct sockaddr *) &addr, &addrlen);
                if (conn_sock == -1) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                clientAddressStr = inet_ntoa(addr.sin_addr);
                info("server: got connection from %s, fd %d\n", clientAddressStr, conn_sock);
                setnonblocking(conn_sock);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock,
                        &ev) == -1) {
                    perror("epoll_ctl: conn_sock");
                    exit(EXIT_FAILURE);
                }
            } else {
                do_use_fd(events[n].data.fd, read_buffer, input_queue);
            }
        }
    }
}

void setnonblocking(int conn_sock) {
    fcntl(conn_sock, F_SETFL, O_NONBLOCK | O_NDELAY);
}

void put_stomp_messages_on_queue(int conn_sock, char* read_buffer, ts_queue * input_queue, int received_length) {
    read_buffer[received_length] = '\0';
    char * token = strtok(read_buffer, "\0");
    while (token != NULL) {
        if (token > (read_buffer + received_length)) break;
        message * incoming_message = message_create(
                conn_sock,
                token);

        if (ts_enqueue_limited(input_queue,
                incoming_message,
                config->max_input_queue_size
                ) < 0)
            warn("server: Dropping message, input_queue limit reached! (%d)\n",
                config->max_input_queue_size);
        token = strtok(NULL, "\0");
    }
}

void close_connection(int conn_sock, ts_queue * input_queue) {

    close(conn_sock);

    int fd_with_flags = session_storage_get(conn_sock);
    if (fd_with_flags != 0) {
        fd_with_flags = session_set_cmd_purge(fd_with_flags);
        char * CMD = clone_str("CMD");

        put_stomp_messages_on_queue(fd_with_flags,
                CMD,
                input_queue,
                strlen(CMD));
        free(CMD);
    }

    clean_by_fd(conn_sock);
}

void do_use_fd(int conn_sock, char* read_buffer, ts_queue * input_queue) {
    int received_length = recv(conn_sock, read_buffer,
            config->input_buffer_size, 0);
    if (received_length < 1) {
        if (received_length == 0) {
            info("server: socket closed nicely. fd:%d\n", conn_sock);
        } else {
            info("server: socked closed with error: %s\n", strerror(errno))
        }
        // TODO: epoll_ctl EPOLL_CTL_DEL ????
        close_connection(conn_sock, input_queue);
    } else {
        if (session_storage_add_new(conn_sock) == MAX_SESSION_NUMBER_EXCEEDED) {
            warn("server: max session size exceeded, dropping connection on fd:%llu\n", conn_sock);
            // Sorry-sorry, no bananas left. :)
            close(conn_sock);
            return;
        }

        size_t decoded_buf_len;
        char * decoded_messages;
        int fd_with_flag;

        switch (ws_input_filter_dataframe(conn_sock, read_buffer,
                received_length,
                &decoded_messages, &decoded_buf_len)) {
            case WS_COMPLETE_DATAFRAME:
                fd_with_flag = session_set_encoded(conn_sock);
                put_stomp_messages_on_queue(fd_with_flag,
                        decoded_messages,
                        input_queue,
                        (int) decoded_buf_len);
                free(decoded_messages);
                break;
            case WS_NOT_A_DATAFRAME:
                put_stomp_messages_on_queue(conn_sock,
                        read_buffer,
                        input_queue,
                        received_length);
                break;
                //case WS_CLIENTS_WANT_TO_CLOSE:
            case WS_BUFFER_EXCEEDED_MAX:
            case WS_TOO_LARGE_DATAFRAME:
                warn("server: dropping websocket data-frame, closing conn on fd:%llu\n", conn_sock);
                close_connection(conn_sock, input_queue);
                free(decoded_messages);
                break;
            case WS_INCOMPLETE_DATAFRAME:
                // do nothing, buffering is made
                info("server: Incomplete dataframe, waiting to continue... on fd:%llu\n", conn_sock);
                break;
        }

    }
}

void accept_incoming_data_loop(int listen_sock_fd) {
    stomp_app_config * config = config_get_config();

    accept_epoll(config, listen_sock_fd);
}

/** get sockaddr, IPv4 or IPv6 */
void *get_inbound_address(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *) sa)->sin_addr);

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}
