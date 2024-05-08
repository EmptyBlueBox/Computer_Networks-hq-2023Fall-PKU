#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include "ftp_server_utils.h"
#include "ftp_server_functions.h"

int main(int argc, char *argv[])
{
    /*
    build a socket
    */
    int sock_fd;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Error: socket() failed\n");
        perror("socket");
        return -1;
    }

    /*
    bind this socket to a port
    */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(u_int16_t(atoi(argv[2]))); // port number
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);     // IP address
    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "Error: bind() failed\n");
        perror("bind");
        return -1;
    }

    /*
    listen to this port
    */
    if (listen(sock_fd, LISTENQ) < 0)
    {
        fprintf(stderr, "Error: listen() failed\n");
        perror("listen");
        return -1;
    }

    /*
    accept connections
    */
    int conn_fd; // 可能产生多次数据交换，要写在while循环外面
    while (true)
    {
        if (status == FTP_SERVER_STATUS::IDLE)
            if ((conn_fd = accept(sock_fd, NULL, NULL)) < 0)
            {
                fprintf(stderr, "Error: accept() failed\n");
                perror("accept");
                return -1;
            }

        /*
        服务器端接收 client 报文，至少接收完报文头
        */
        DATA_STREAM *client_request = (DATA_STREAM *)malloc(sizeof(DATA_STREAM));
        int already_received = 0, expect_length = HEADER_SIZE;
        while (already_received < expect_length)
        {
            int received = recv(conn_fd, (u_int8_t *)client_request + already_received, expect_length - already_received, 0);
            if (received < 0)
            {
                fprintf(stderr, "Error: recv() failed\n");
                perror("recv");
                free(client_request);
                return -1;
            }
            else if (received == 0)
            {
                fprintf(stderr, "Error: server closed connection\n");
                free(client_request);
                return -1;
            }
            already_received += received;
        }

        /*
        检查请求报文头是否合法
        */
        if (check_protocol(client_request) == false)
        {
            fprintf(stderr, "Error: file data response protocol number is not correct\n");
            free(client_request);
            return -1;
        }

        /*
        分配足够大的内存继续接收请求报文体 entity body，使用 realloc() 扩充内存
        */
        client_request = (DATA_STREAM *)realloc(client_request, ntohl(client_request->m_length));
        expect_length = ntohl(client_request->m_length);
        while (already_received < expect_length)
        {
            int received = recv(conn_fd, (u_int8_t *)client_request + already_received, expect_length - already_received, 0);
            if (received < 0)
            {
                fprintf(stderr, "Error: recv() failed\n");
                perror("recv");
                free(client_request);
                return -1;
            }
            else if (received == 0)
            {
                fprintf(stderr, "Error: server closed connection\n");
                free(client_request);
                return -1;
            }
            already_received += received;
        }

        /*
        回复 client 报文
        */
        switch (client_request->m_type)
        {
        case COMMAND_TYPE::OPEN_CONN_REQUEST:
            if (server_open(conn_fd) < 0)
            {
                fprintf(stderr, "Error: server_open() failed\n");
                free(client_request);
            }
            break;
        case COMMAND_TYPE::LIST_REQUEST:
            if (server_ls(conn_fd) < 0)
            {
                fprintf(stderr, "Error: server_ls() failed\n");
                free(client_request);
            }
            break;
        case COMMAND_TYPE::GET_REQUEST:
            if (server_get(client_request->payload, conn_fd) < 0)
            {
                fprintf(stderr, "Error: server_get() failed\n");
                free(client_request);
            }
            break;
        case COMMAND_TYPE::PUT_REQUEST:
            if (server_put(client_request->payload, conn_fd) < 0)
            {
                fprintf(stderr, "Error: server_put() failed\n");
                free(client_request);
            }
            break;
        case COMMAND_TYPE::SHA_REQUEST:
            if (server_sha256(client_request->payload, conn_fd) < 0)
            {
                fprintf(stderr, "Error: server_sha() failed\n");
                free(client_request);
            }
            break;
        case COMMAND_TYPE::QUIT_REQUEST:
            if (server_quit(conn_fd) < 0)
            {
                fprintf(stderr, "Error: server_quit() failed\n");
                free(client_request);
            }
            break;
        default:
            fprintf(stderr, "Error: invalid command\n");
            free(client_request);
        }
    }

    close(sock_fd);
    return 0;
}