/*
 * server.c
 * Version 20161003
 * Written by Harry Wong (RedAndBlueEraser)
 */

#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "camera.h"

#define BACKLOG 10

typedef struct pthread_arg_t {
    int new_socket_fd;
    struct sockaddr_in client_address;
    /* TODO: Put arguments passed to threads here. See lines 116 and 139. */
} pthread_arg_t;

/* Thread routine to serve connection to client. */
void *pthread_routine(void *arg);

/* Signal handler to handle SIGTERM and SIGINT signals. */
void signal_handler(int signal_number);

/* socket */
int socket_fd = 0;
int active = 0;
pthread_t pthread;

/* main function */
int main(int argc, char *argv[]) {
    int port, new_socket_fd;
    struct sockaddr_in address;
    pthread_attr_t pthread_attr;
    pthread_arg_t *pthread_arg;
    socklen_t client_address_len;

    /* Get port from command line arguments or stdin. */
    port = argc > 1 ? atoi(argv[1]) : 0;
    if (!port) {
        printf("Enter Port: ");
        scanf("%d", &port);
    }

    /* Initialise IPv4 address. */
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    /* Create TCP socket. */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    /* Bind address to socket. */
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof address) == -1) {
        perror("bind");
        exit(1);
    }

    /* Listen on socket. */
    if (listen(socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    /* Assign signal handlers to signals. */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        exit(1);
    }
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    /* Initialise pthread attribute to create detached threads. */
    if (pthread_attr_init(&pthread_attr) != 0) {
        perror("pthread_attr_init");
        exit(1);
    }
    if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("pthread_attr_setdetachstate");
        exit(1);
    }

    while (1) {
        /* Create pthread argument for each connection to client. */
        /* TODO: malloc'ing before accepting a connection causes only one small
         * memory when the program exits. It can be safely ignored.
         */
        pthread_arg = (pthread_arg_t *)malloc(sizeof *pthread_arg);
        if (!pthread_arg) {
            perror("malloc");
            continue;
        }

        /* Accept connection to client. */
        client_address_len = sizeof(pthread_arg->client_address);
        new_socket_fd = accept(socket_fd, (struct sockaddr *)&pthread_arg->client_address, &client_address_len);
        if (new_socket_fd == -1) {
            perror("accept");
            free(pthread_arg);
            continue;
        }

        /* Initialise pthread argument. */
        pthread_arg->new_socket_fd = new_socket_fd;
        /* TODO: Initialise arguments passed to threads here. See lines 22 and
         * 139.
         */

        /* Create thread to serve connection to client. */
        active = 1;
        if (pthread_create(&pthread, &pthread_attr, pthread_routine, (void *)pthread_arg) != 0) {
            perror("pthread_create");
            free(pthread_arg);
            continue;
        }
    }

    /* close(socket_fd);
     * TODO: If you really want to close the socket, you would do it in
     * signal_handler(), meaning socket_fd would need to be a global variable.
     */
    close(socket_fd);
    return 0;
}

int socket_select(int fd, int usec_timeout);
void read_user_name(int socket_fd, int length);
void read_video_data(int socket_fd, int length, int video_frame_no);
void write_open_device(int socket_fd, camera_event_packet_header* header);
void write_close_device(int socket_fd, camera_event_packet_header* header);
void msleep(int delay);

void *pthread_routine(void *arg) {
    pthread_arg_t *pthread_arg = (pthread_arg_t *)arg;
    int new_socket_fd = pthread_arg->new_socket_fd;
    struct sockaddr_in client_address = pthread_arg->client_address;
    /* TODO: Get arguments passed to threads here. See lines 22 and 116. */

    free(arg);

    /* Print log */
    printf("thread is running\n");

    /* Initialize */
    int retval = 0, length = 0;
    int received_user_name = 0, already_sent_open_device = 0, already_sent_close_device = 0;
    int receive_video_frame_count = 0;
    camera_event_packet_header header;
    memset(&header, 0, sizeof(camera_event_packet_header));

    /* Event Loop */
    while (active == 1) {

        /* No instruction yet? Get more data ... */
        retval = socket_select(new_socket_fd, 10000);
        if (retval > 0) {
            /* Read the packet */
            retval = read(new_socket_fd, &header, sizeof(camera_event_packet_header));
            if (retval == 0) {
                printf("socket is disconnected.\n");
                break;
            }

            /* Process camera event */
            switch (header.event) {
                case USER_NAME:
                    length = header.length - sizeof(camera_event_packet_header);
                    read_user_name(new_socket_fd, length);
                    received_user_name = 1;
                    break;

                case VIDEO_DATA:
                    length = header.length - sizeof(camera_event_packet_header);
                    read_video_data(new_socket_fd, length, receive_video_frame_count);
                    receive_video_frame_count++;
                    break;
            }
        }
        
        /* Reset the packet */
        memset(&header, 0, sizeof(camera_event_packet_header));

        /* Send the open device */
        if (received_user_name && !already_sent_open_device) {
            msleep(10000);
            write_open_device(new_socket_fd, &header);
            already_sent_open_device = 1;
        }

        /* Send the close device */
        if (already_sent_open_device
                && receive_video_frame_count == MAX_RECEIVED_VIDEO_COUNT
                && !already_sent_close_device) {
            msleep(1000);
            write_close_device(new_socket_fd, &header);
            already_sent_close_device = 1;
        }

    }

    close(new_socket_fd);
    return NULL;
}

int socket_select(int fd, int usec_timeout) {

    fd_set fds;

    /* Initialize fd_set with single underlying file descriptor */
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    /* No timeout if usec_timeout is negative */
    if (usec_timeout < 0)
        return select(fd + 1, &fds, NULL, NULL, NULL); 

    /* Handle timeout if specified */
    struct timeval timeout = {
        .tv_sec  = usec_timeout / 1000000,
        .tv_usec = usec_timeout % 1000000
    };

    return select(fd + 1, &fds, NULL, NULL, &timeout);

}

void read_user_name(int socket_fd, int length) {

    char* user_name = calloc(1, length + 1);

    read(socket_fd, user_name, length + 1);
    printf("Received user name: %s\n", user_name);

    free(user_name);
    user_name = NULL;

}

void read_video_data(int socket_fd, int length, int video_frame_no) {

    uint8_t* video_data = NULL;
    int total_length = 0, read_length = 0;

    video_data = calloc(1, length);
    while (total_length < length) {
        read_length = read(socket_fd, video_data + total_length, length - total_length);
        total_length += read_length;
    }

    printf("%02d: Received video data: total_length=%d, packet_length=%d\n",
        video_frame_no + 1, total_length, length);

    free(video_data);
    video_data = NULL;

}

void write_open_device(int socket_fd, camera_event_packet_header* header) {

    header->event = OPEN_DEVICE;
    header->length = sizeof(camera_event_packet_header);

    write(socket_fd, header, header->length);
    printf("Sent open_device event.\n");

}

void write_close_device(int socket_fd, camera_event_packet_header* header) {

    header->event = CLOSE_DEVICE;
    header->length = sizeof(camera_event_packet_header);

    write(socket_fd, header, header->length);
    printf("Sent close_device event.\n");

}

void msleep(int delay) {

    struct timespec sleep = {
        .tv_sec  =  delay / 1000,
        .tv_nsec = (delay % 1000) * 1000000
    };

    nanosleep(&sleep, NULL);

}

void signal_handler(int signal_number) {
    /* TODO: Put exit cleanup code here. */
    if (active == 1) {
        active = 0;
        pthread_join(pthread, NULL);
        printf("thread is closed\n");
    }

    close(socket_fd);
    exit(0);
}
