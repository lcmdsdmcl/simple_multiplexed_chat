/*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <time.h>
#include <fcntl.h>

#define BUF_LEN 256
#define PORT 9000

static void error_hndlr(const char *get) {
    fputs(strerror(errno), stderr);
    fputs(": ", stderr);
    fputs(get, stderr);
    fputs("\n", stderr);

    exit(EXIT_FAILURE);
}

void timestamp(char *buf) {
    time_t td;
    char time_buf[128];
    int n;

    time(&td);
    n = (int)strftime(time_buf, sizeof time_buf, "%H %M %S", localtime(&td));
    // error handling

    memcpy(buf, time_buf, sizeof time_buf);
}


int main() {

    int z;  // temp return value
    int reuse = 1; // optval true
    int fd; // fd for iteration
    int i; // temp variable for loop
    int flags; // flags for fctnl
    struct sockaddr_in srvr_addr, clnt_addr;
    fd_set master_fds, other_fds;
    int max_fd;
    int server_socket, client_socket;
    size_t server_len, client_len;
    char buf[BUF_LEN];
    char msg[BUF_LEN];
    char time[BUF_LEN];

    memset(&buf, 0, sizeof buf); // zero out
    memset(&msg, 0, sizeof msg);
    memset(&time, 0, sizeof time);

    // create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
        error_hndlr("Could not open socket()");

    // enable address reuse
    z = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
    if (z == -1)
        error_hndlr("Could not reuse address with setsockopt()");

    memset(&srvr_addr, 0, sizeof srvr_addr);
    srvr_addr.sin_family = AF_INET;
    srvr_addr.sin_addr.s_addr = INADDR_ANY;
    srvr_addr.sin_port = htons(PORT);
    server_len = sizeof srvr_addr;

    // bind addr to socket
    z = bind(server_socket, (struct sockaddr*)&srvr_addr, server_len);
    if (z == -1)
        error_hndlr("Could not bind()");

    // listen
    z = listen(server_socket, 10);
    if(z == -1)
        error_hndlr("Could not listen()");

    // saves flags before manipulating them, => should create a set_nonblock()
    if ((flags = fcntl(server_socket, F_GETFL, 0)) < 0)
        error_hndlr("Could not save fcntl() flags");

    if (fcntl(server_socket, F_SETFL, flags | O_NONBLOCK) < 0)
        error_hndlr("Could not set fcntl() NONBLOCK flag");

    // initialize and set the file descriptors
    FD_ZERO(&master_fds);
    FD_ZERO(&other_fds);

    FD_SET(server_socket, &master_fds);

    max_fd = server_socket;

    for(;;) {
        // copy master_fds to other_fds
        memcpy(&other_fds, &master_fds, sizeof master_fds);

        // Synchronous I/O Multiplexing, monitoring a set of fds
        z = select(max_fd+1, &other_fds, 0,0,0);
        if (z == 0)
            error_hndlr("select() timeout");
        else if (z == -1)
            error_hndlr("select() failed");

        for(fd = 0; fd <= max_fd; fd++) {
            // if the given fd is part of other set
            if( FD_ISSET(fd, &other_fds) ) {
            // if fd is server -> accept
                if(fd == server_socket) {
                    client_len = sizeof clnt_addr;
                    client_socket = accept(server_socket, (struct sockaddr*)&clnt_addr, &client_len);
                    if (client_socket == -1)
                        error_hndlr("Could not accept() client socket");

                    // saves flags before manipulating them
                    if ((flags = fcntl(client_socket, F_GETFL, 0)) < 0)
                        error_hndlr("Could not save fcntl() flags");

                    if (fcntl(client_socket, F_SETFL, flags | O_NONBLOCK) < 0)
                        error_hndlr("Could not set fcntl() NONBLOCK flag");

                    // add to the master file descriptors set
                    FD_SET(client_socket, &master_fds);
                    // if the client's fd is higher than the highest fd
                    if(client_socket > max_fd)
                        max_fd = client_socket;


                } else { // if fd is not the server socket
                    z = recv(fd, buf, sizeof buf, 0); // could use read
                    if (z <= 0) { // if nothing's been read
                        close(fd);
                        FD_CLR(fd, &master_fds);
                        //error_hndlr("recv() nothing");
                        printf("recv() nothing, somebody's quit \n");
                    }

                    timestamp(time);
                    // copy timestamp and recv buffer inmsg
                    snprintf(msg, sizeof msg, "[%s] %s", time, buf);
                    // sends message to everybody except to server
                    for(i = 0; i <= max_fd; i++) {
                        if(FD_ISSET(i, &master_fds)) {
                            if(i != server_socket) {
                                z = send(i, msg, sizeof msg, 0); // could use write
                                if (z == -1)
                                    error_hndlr("could not send() message");
                            }
                        }
                    }
                }
            }
        }
    }


    close(server_socket);

    return 0;
}
