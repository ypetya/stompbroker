
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
#include "../websocket/filter/handshake.h"
#include "../websocket/buffer.h"
#include "../stomp/data_wrappers/pub_sub.h"
#include "../stomp/framing.h"
#include "data/message/with_timestamp.h"
#include "data/cleanup.h"

#include <string.h> // memcpy

#include <time.h>


void *get_inbound_address(struct sockaddr *sa);

void setnonblocking(int conn_sock);
void do_use_fd(int epollfd, int fd, char * readbuffer, ts_queue * input_queue);
void close_connection(int epollfd, int conn_sock, ts_queue * input_queue);

stomp_app_config * config;

long long now; // receive time of the current epoll round (TTL)

void accept_epoll(stomp_app_config * app_config, int listen_sock) {
    config = app_config;

    char read_buffer[config->input_buffer_size + 1];
    read_buffer[config->input_buffer_size] = '\0';
    ws_init_buffer();
    ts_queue * input_queue = process_start_threads();


#define MAX_EVENTS 10
    struct epoll_event ev = (struct epoll_event){}, events[MAX_EVENTS];
    int conn_sock, epollfd;
    struct sockaddr_in addr; // connector's address information
    socklen_t addrlen = sizeof addr;
    char * clientAddressStr; //[INET6_ADDRSTRLEN];

    for (int i = 0; i < MAX_EVENTS; i++) events[i] = (struct epoll_event){};
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
        int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);

        if (nfds < 0) {
            perror("epoll_wait problem");

            continue;
        }

        now = message_timestamp_now();

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
                ws_conn_reset(conn_sock); // fd numbers are reused: drop stale transport state
                stomp_input_reset(stomp_input_of(conn_sock));
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock,
                        &ev) == -1) {
                    perror("epoll_ctl: conn_sock");
                    exit(EXIT_FAILURE);
                }
            } else {
                do_use_fd(epollfd, events[n].data.fd, read_buffer, input_queue);
            }
        }
    }
}

void setnonblocking(int conn_sock) {
    fcntl(conn_sock, F_SETFL, O_NONBLOCK | O_NDELAY);
}

static void enqueue_input(ts_queue * input_queue, message_with_timestamp * incoming_message) {
    if (ts_enqueue_limited(input_queue,
            incoming_message,
            config->max_input_queue_size
            ) < 0) {

        warn("server: Dropping message, input_queue limit reached! (%d)\n",
                config->max_input_queue_size);

        message_destroy_with_timestamp(incoming_message);
    }
}

typedef struct frame_target_st {
    int fd_with_flags;
    ts_queue * input_queue;
} frame_target;

/** Input of the current session of fd: replies inherit its generation. */
static message_with_timestamp * received(int fd_with_flags, const char * frame, size_t len) {
    message_with_timestamp * m = message_create_with_timestamp_len(fd_with_flags, frame, len, now);
    m->generation = session_storage_generation(fd_with_flags);
    return m;
}

static void enqueue_frame(void * ctx, const char * frame, size_t len) {
    frame_target * target = ctx;
    enqueue_input(target->input_queue, received(target->fd_with_flags, frame, len));
}

/**
 * Cuts received bytes into STOMP frames (they may span reads, several may
 * arrive in one read) and queues every complete frame for the reader thread.
 * On a framing error the reader sends ERROR and the connection is closed;
 * further input is ignored until then.
 */
void put_stomp_messages_on_queue(int fd_with_flags, const char* data, ts_queue * input_queue, size_t received_length) {
    int conn_sock = session_without_flags(fd_with_flags);
    frame_target target = {fd_with_flags, input_queue};
    const char * error = NULL;

    if (stomp_input_feed(stomp_input_of(conn_sock), data, received_length, config->max_frame_size,
            enqueue_frame, &target, &error) < 0) {
        warn("server: STOMP framing error (%s), closing fd:%d\n", error, conn_sock);
        ws_conn_set_state(conn_sock, CONN_WS_CLOSING); // ignore input until the connection is closed
        message_with_timestamp * failure = received(fd_with_flags, "", 0);
        failure->error = error;
        ts_enqueue(input_queue, failure); // not dropped: the client must learn why it is closed
    }
}

void close_connection(int epollfd, int conn_sock, ts_queue * input_queue) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, conn_sock, NULL);
    connection_close(conn_sock, input_queue, process_output_queue());
}

/** Sends pre-encoded bytes through the writer threads (keeps output single-pathed). */
void queue_raw_output(int conn_sock, const char *bytes, size_t len, int close_after) {
    message_with_frame_len * m = message_create_raw(conn_sock, bytes, len, close_after);
    m->generation = session_storage_generation(conn_sock);
    ts_enqueue(process_output_queue(), m);
}

/**
 * Runs bytes of a WebSocket connection through the frame filter: complete
 * messages go to the STOMP input queue, a close (client or protocol error)
 * closes the connection.
 */
void handle_websocket_data(int conn_sock, const char *data, int received_length, ts_queue * input_queue) {
    ws_input_result result;
    ws_filter_dataframe_status status = ws_input_filter_dataframe(conn_sock, data,
            (size_t) received_length, WS_DATA_FRAME_MAX_LENGTH, &result);

    if (result.messages_len > 0)
        put_stomp_messages_on_queue(session_set_encoded(conn_sock),
            result.messages,
            input_queue,
            result.messages_len); // every message is '\0' terminated

    if (result.replies_len > 0)
        queue_raw_output(conn_sock, result.replies, result.replies_len, result.close);

    if (result.close) {
        // The writer closes TCP after the close frame is out; ignore input until then.
        ws_conn_set_state(conn_sock, CONN_WS_CLOSING);
        switch (status) {
            case WS_OPCODE_CLIENT_DISCONNECT:
                info("server: got websocket close (%d) from client. disconnecting fd:%d\n", result.close_code, conn_sock);
                break;
            case WS_BUFFER_OUT_OF_SLOTS:
            case WS_BUFFER_EXCEEDED_MAX:
                warn("server: websocket buffer exceeded. disconnecting fd:%d\n", conn_sock);
                break;
            case WS_TOO_LARGE_DATAFRAME:
                warn("server: got too large websocket message. disconnecting fd:%d\n", conn_sock);
                break;
            default:
                warn("server: websocket protocol error (%d). disconnecting fd:%d\n", result.close_code, conn_sock);
                break;
        }
    }

    ws_input_result_free(&result);
}

/**
 * HTTP upgrade request (possibly split over several reads). On success the
 * session is marked encoded and switches to WebSocket framing; on failure the
 * HTTP error is sent and the writer closes the connection.
 */
void handle_handshake_data(int conn_sock, const char *data, int received_length, ts_queue * input_queue) {
    ws_handshake_policy policy = {config->ws_allowed_origins, config->ws_allowed_hosts};
    ws_handshake_result result;
    ws_handshake_status status = ws_input_filter_handshake(conn_sock, data, (size_t) received_length,
            &policy, &result);

    if (status == WS_HANDSHAKE_OK) {
        session_storage_lock();
        session_storage_set_encoded(conn_sock);
        session_storage_unlock();
        ws_conn_set_state(conn_sock, CONN_WS_OPEN);
        queue_raw_output(conn_sock, result.response, result.response_len, 0);
        info("server: websocket handshake done%s%s. fd:%d\n",
                result.protocol[0] ? ", subprotocol " : "", result.protocol, conn_sock);

        buffer_item *pending = ws_buffer_find(conn_sock); // frames sent right after the request
        if (pending != NULL && pending->received_len > 0)
            handle_websocket_data(conn_sock, "", 0, input_queue);
    } else if (status != WS_HANDSHAKE_INCOMPLETE) {
        warn("server: websocket handshake rejected (%d). fd:%d\n", status, conn_sock);
        ws_conn_set_state(conn_sock, CONN_WS_CLOSING);
        queue_raw_output(conn_sock, result.response, result.response_len, 1);
    }

    ws_handshake_result_free(&result);
}

/**
 * Routes one chunk of received bytes by the connection's transport state.
 * @return 0 if the connection was dropped
 */
int process_received(int conn_sock, char * read_buffer, int received_length, ts_queue * input_queue) {
    if (ws_conn_get_state(conn_sock) == CONN_WS_CLOSING) return 1; // closing handshake in progress

    session_storage_lock();
    if (session_storage_add_new(conn_sock) == MAX_SESSION_NUMBER_EXCEEDED) {
        warn("server: max session size exceeded, dropping connection on fd:%d\n", conn_sock);
        // Sorry-sorry, no bananas left. :)
        session_storage_unlock();
        close(conn_sock);
        ws_conn_reset(conn_sock);
        stomp_input_reset(stomp_input_of(conn_sock));
        return 0;
    }
    session_storage_unlock();

    switch (ws_conn_route(conn_sock, read_buffer, (size_t) received_length)) {
        case CONN_RAW_STOMP:
            put_stomp_messages_on_queue(conn_sock, read_buffer, input_queue, (size_t) received_length);
            break;
        case CONN_HTTP_PENDING:
            handle_handshake_data(conn_sock, read_buffer, received_length, input_queue);
            break;
        case CONN_WS_OPEN:
            handle_websocket_data(conn_sock, read_buffer, received_length, input_queue);
            break;
        default:
            break;
    }
    return 1;
}

/** Edge triggered: drain the socket until it would block. */
void do_use_fd(int epollfd, int conn_sock, char* read_buffer, ts_queue * input_queue) {
    for (;;) {
        int received_length = recv(conn_sock, read_buffer,
                config->input_buffer_size, 0);
        if (received_length < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
        if (received_length < 0 && errno == EINTR) continue;

        if (received_length < 1) {
            if (received_length == 0) {
                info("server: socket closed nicely. fd:%d\n", conn_sock);
            } else {
                info("server: socked closed with error: %s\n", strerror(errno))
            }

            close_connection(epollfd, conn_sock, input_queue);
            return;
        }

        if (!process_received(conn_sock, read_buffer, received_length, input_queue)) return;
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
