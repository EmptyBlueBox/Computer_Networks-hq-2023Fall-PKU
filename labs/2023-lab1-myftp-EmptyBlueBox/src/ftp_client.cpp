#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "ftp_client_utils.h"
#include "ftp_client_functions.h"

int main(int argc, char **argv)
{
    int sock_fd;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Error: socket() failed\n");
        perror("socket");
        return -1;
    }

    while (true)
    {
        printf("Client (%s) > ", connecting.c_str());
        char command[BUF_SIZE];
        if (fgets(command, BUF_SIZE, stdin) == NULL)
        {
            fprintf(stderr, "Error: fgets() failed\n");
            perror("fgets");
        }

        FTP_CLIENT_COMMAND cmd = parse_command(command);
        switch (cmd)
        {
        case FTP_CLIENT_COMMAND::OPEN:
            if (client_open(command, sock_fd))
                fprintf(stderr, "Error: client_open() failed\n");
            break;
        case FTP_CLIENT_COMMAND::LS:
            if (client_ls(sock_fd))
                fprintf(stderr, "Error: client_ls() failed\n");
            break;
        case FTP_CLIENT_COMMAND::GET:
            if (client_get(command, sock_fd))
                fprintf(stderr, "Error: client_get() failed\n");
            break;
        case FTP_CLIENT_COMMAND::PUT:
            if (client_put(command, sock_fd))
                fprintf(stderr, "Error: client_put() failed\n");
            break;
        case FTP_CLIENT_COMMAND::SHA256:
            if (client_sha256(command, sock_fd))
                fprintf(stderr, "Error: client_sha256() failed\n");
            break;
        case FTP_CLIENT_COMMAND::QUIT:
            if (client_quit(sock_fd))
                fprintf(stderr, "Error: client_close() failed\n");
            else if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) // reset socket because socket was closed
            {
                fprintf(stderr, "Error: socket() failed\n");
                perror("socket");
                return -1;
            }
            break;
        case FTP_CLIENT_COMMAND::INVALID:
            fprintf(stderr, "Error: invalid command\n");
            break;
        }
    }

    close(sock_fd);
    return 0;
}