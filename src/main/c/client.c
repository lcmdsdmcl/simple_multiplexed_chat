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

#define BUFF_LEN 256
#define SRVR_ADDR "127.0.0.1"
#define PORT 9000

static void error_hndlr(const char *get) {
    fputs(strerror(errno), stderr);
    fputs(": ", stderr);
    fputs(get, stderr);
    fputs("\n", stderr);

    exit(EXIT_FAILURE);
}

int main() {

    int z; // temp return value
    int is_data; // is there any data?
    int client_socket; // client socket descriptor
    size_t len;
    int fd;

    struct sockaddr_in addr; // socket address
    struct timeval time; // timeval for select()

    fd_set master_fds; // fd sets
    fd_set other_fds;

    char buf[BUFF_LEN];
    char msg[BUFF_LEN];
    char name[BUFF_LEN];

    memset(&buf, 0, sizeof buf); // zero out
    memset(&msg, 0, sizeof msg);

    // get name
    do {
        printf("Name：");
        z = scanf("%s", name);
    } while (z != 1);

    // creating socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
        error_hndlr("Could not open socket()");

    // address
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(SRVR_ADDR);
    addr.sin_port = htons(PORT);
    len = sizeof addr;

    // creating connection
    z = connect(client_socket, (struct sockaddr *) &addr, len);
    if (z == -1)
        error_hndlr("Could not connect()");

    fd = fileno(stdin); // fd = standard input descr.

    FD_ZERO(&master_fds);
    FD_SET(client_socket, &master_fds);

    time.tv_sec = 0;
    time.tv_usec = 1000;

    for(;;) {

        FD_ZERO(&other_fds);
        FD_SET(fd, &other_fds);

        if (select(fd + 1, &other_fds, 0, 0, &time)) {
            fgets(buf, BUFF_LEN, stdin);

            snprintf(msg, sizeof msg, "%s: %s", name, buf);

            z = send(client_socket, msg, sizeof msg, 0);
            if (z == -1)
                error_hndlr("Could not send()");

        }

        ioctl(client_socket, FIONREAD, &is_data); // is there anything to read?
        if (is_data != 0) {

            if (FD_ISSET(client_socket, &master_fds)) {
                z = recv(client_socket, buf, sizeof buf, 0);
                if (z == -1)
                    error_hndlr("Could not recv()");

                printf("%s", buf);
            }
        }

    }

    close(client_socket);

    return 0;
}

