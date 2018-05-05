/***************************************************************************/
/*                                                                         */
/* Server program which wait for the client to connect and reads the data  */
/*     using non-blocking socket.                                          */
/* The reading of non-blocking sockets is done in a loop until data        */
/*     arrives to the sockfd.                                              */
/*                                                                         */
/* based on Beej's program - look in the simple TCP server for further doc.*/
/*                                                                         */
/*                                                                         */
/***************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h> // warning inet_ntoa declared here
#include <sys/wait.h>
#include <fcntl.h> /* Added for the nonblocking socket */
#include <unistd.h> // for usleep
#include <errno.h>

#define MYPORT 3456    /* the port users will be connecting to */
#define BACKLOG 10     /* how many pending connections queue will hold */

int main()
{
    int 			sockfd, new_fd;  /* listen on sock_fd, new connection on new_fd */
    struct 	sockaddr_in 	my_addr;    /* my address information */
    struct 	sockaddr_in 	their_addr; /* connector's address information */
    //int 			sin_size;
    socklen_t		sin_size;
    char			string_read[255];
    char			string_write[255];
    int 			n,i;
    int			last_fd;	/* Thelast sockfd that is connected	*/

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        printf("error: %s",strerror(errno));
        exit(1);
    }

    last_fd = sockfd;

    my_addr.sin_family = AF_INET;         /* host byte order */
    my_addr.sin_port = htons(MYPORT);     /* short, network byte order */
    my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */
    bzero(&(my_addr.sin_zero), 8);        /* zero the rest of the struct */

//?? place sure
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        printf("setsockopt(SO_REUSEADDR) failed");

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        printf("error: %s",strerror(errno));
        exit(1);
    }




    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        printf("error: %s",strerror(errno));

        exit(1);
    }
/*
    sin_size = sizeof(struct sockaddr);
    if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr,&sin_size)) == -1) {
        perror("accept1");
        printf("error: %s",strerror(errno));

    }
*/
//printf("last_fd= %d\n",last_fd);
//printf("sockfd= %d\n",sockfd);
//printf("new_fd= %d\n",new_fd);



    fcntl(last_fd, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state	*/
//    fcntl(new_fd, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state	*/

    while(1){
        for (i=sockfd;i<=last_fd;i++){
            //printf("Round number %d\n",i);
            if (i == sockfd){
                sin_size = sizeof(struct sockaddr_in);
                if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr,&sin_size)) == -1) {
                    //perror("accept2");
                    //printf("error: %s",strerror(errno));
                }else{
                    printf("server: got connection from %s\n", (char *)inet_ntoa(their_addr.sin_addr));
                    fcntl(new_fd, F_SETFL, O_NONBLOCK);
                    last_fd = new_fd;
                    //printf("new_fd= %d\n",new_fd);

                }
            }
            else{
                n=recv(i,string_read,sizeof(string_read),0);
                if (n < 1){
                    //if(errno == EWOULDBLOCK){printf("EWOULDBLOCK");}
                    //if(errno == EAGAIN){printf("EAGAIN   ");}
                    if(errno != EWOULDBLOCK){
                        perror("recv - non blocking \n");
                        printf("Round %d, and the data read size is: n=%d \n",i,n);
                    }
                }
                else{
                    string_read[n] = '\0';
                    printf("The string is: %s \n",string_read);
                    if(strlen(string_read) < (sizeof(string_write) - 10) ){
                        sprintf(string_write,"You said:%s\n",string_read);
                    }else{
                        strcpy(string_write,"Jabber...\n");

                        }
                    if (send(i, string_write, strlen(string_write), 0) == -1)
                        perror("send");
                }
            }
        }
        usleep(1000000);
    }

    return 0;
}

