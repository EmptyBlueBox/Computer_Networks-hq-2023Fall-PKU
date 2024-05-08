#include "ftp_server_functions.h"
#include "ftp_server_utils.h"
#include <string>
#include <sstream>
#include <vector>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

int server_open(int sock_fd)
{
    /*
    服务器端组织并发送 open 回复报文
    */
    DATA_STREAM ls_request = {
        .m_protocol = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')},
        .m_type = 0xA2,
        .m_status = 1,
        .m_length = htonl(HEADER_SIZE),
    };

    u_int32_t already_sent = 0, total_length = ntohl(ls_request.m_length);
    while (already_sent < total_length)
    {
        int sent = send(sock_fd, (u_int8_t *)&ls_request + already_sent, total_length - already_sent, 0);
        if (sent < 0)
        {
            fprintf(stderr, "Error: send() failed\n");
            perror("send");
            return -1;
        }
        else if (sent == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            return -1;
        }
        already_sent += sent;
    }

    status = FTP_SERVER_STATUS::CONNECTED;
    return 0;
}

int server_ls(int sock_fd)
{
    /*
    服务器端读取 ls 命令执行结果
    */
    FILE *fp = popen("ls", "r"); // 流文件，不知道大小
    if (fp == NULL)
    {
        fprintf(stderr, "Error: popen() failed\n");
        perror("popen");
        return -1;
    }
    char buffer[BUF_SIZE];
    int ls_size = fread(buffer, 1, BUF_SIZE, fp);
    if (ls_size < 0)
    {
        fprintf(stderr, "Error: fread() failed\n");
        perror("fread");
        return -1;
    }
    pclose(fp);

    /*
    服务器端组织并发送 ls 回复报文
    */
    DATA_STREAM *ls_reply = (DATA_STREAM *)malloc(HEADER_SIZE + ls_size + 1);
    ls_reply->m_protocol[0] = u_int8_t(0xc1), ls_reply->m_protocol[1] = u_int8_t(0xa1), ls_reply->m_protocol[2] = u_int8_t(0x10), ls_reply->m_protocol[3] = u_int8_t('f'), ls_reply->m_protocol[4] = u_int8_t('t'), ls_reply->m_protocol[5] = u_int8_t('p');
    ls_reply->m_type = 0xA4;
    ls_reply->m_status = 0;
    ls_reply->m_length = htonl(HEADER_SIZE + ls_size + 1);
    memcpy(ls_reply->payload, buffer, ls_size);

    int already_sent = 0, total_length = ntohl(ls_reply->m_length);
    while (already_sent < total_length)
    {
        int sent = send(sock_fd, (u_int8_t *)ls_reply + already_sent, total_length - already_sent, 0);
        if (sent < 0)
        {
            fprintf(stderr, "Error: send() failed\n");
            perror("send");
            free(ls_reply);
            return -1;
        }
        else if (sent == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            free(ls_reply);
            return -1;
        }
        already_sent += sent;
        printf("transmitting %lf%% ...\n", (double)already_sent / total_length * 100);
    }
    printf("client ls complete\n");

    free(ls_reply);
    return 0;
}

int server_get(char *file_name, int sock_fd)
{
    if (access(file_name, F_OK) == -1)
    {
        /*
        服务器端组织并发送 get 回复报文
        */
        DATA_STREAM get_reply = {
            .m_protocol = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')},
            .m_type = 0xA6,
            .m_status = 0,
            .m_length = htonl(HEADER_SIZE),
        };

        u_int32_t already_sent = 0, total_length = ntohl(get_reply.m_length);
        while (already_sent < total_length)
        {
            int sent = send(sock_fd, (u_int8_t *)&get_reply + already_sent, total_length - already_sent, 0);
            if (sent < 0)
            {
                fprintf(stderr, "Error: send() failed\n");
                perror("send");
                return -1;
            }
            else if (sent == 0)
            {
                fprintf(stderr, "Error: server closed connection\n");
                return -1;
            }
            already_sent += sent;
        }
    }
    else
    {
        /*
        服务器端组织并发送 get 回复报文
        */
        DATA_STREAM get_reply = {
            .m_protocol = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')},
            .m_type = 0xA6,
            .m_status = 1,
            .m_length = htonl(HEADER_SIZE),
        };

        u_int32_t already_sent = 0, total_length = ntohl(get_reply.m_length);
        while (already_sent < total_length)
        {
            int sent = send(sock_fd, (u_int8_t *)&get_reply + already_sent, total_length - already_sent, 0);
            if (sent < 0)
            {
                fprintf(stderr, "Error: send() failed\n");
                perror("send");
                return -1;
            }
            else if (sent == 0)
            {
                fprintf(stderr, "Error: server closed connection\n");
                return -1;
            }
            already_sent += sent;
        }

        /*
        如果文件存在，服务器端组织文件数据
        */
        FILE *fp = fopen(file_name, "rb");
        if (fp == NULL)
        {
            fprintf(stderr, "Error: fopen() failed\n");
            perror("fopen");
            return -1;
        }
        fseek(fp, 0, SEEK_END);
        u_int32_t file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        /*
        服务器端组织并发送文件
        */
        DATA_STREAM *file_data = (DATA_STREAM *)malloc(HEADER_SIZE + file_size);
        file_data->m_protocol[0] = u_int8_t(0xc1), file_data->m_protocol[1] = u_int8_t(0xa1), file_data->m_protocol[2] = u_int8_t(0x10), file_data->m_protocol[3] = u_int8_t('f'), file_data->m_protocol[4] = u_int8_t('t'), file_data->m_protocol[5] = u_int8_t('p');
        file_data->m_type = 0xFF;
        file_data->m_status = 0;
        file_data->m_length = htonl(HEADER_SIZE + file_size);
        if (fread(file_data->payload, 1, file_size, fp) != file_size)
        {
            fprintf(stderr, "Error: fread() failed\n");
            perror("fread");
            free(file_data);
            return -1;
        }
        fclose(fp);

        already_sent = 0, total_length = ntohl(file_data->m_length);
        while (already_sent < total_length)
        {
            int sent = send(sock_fd, (u_int8_t *)file_data + already_sent, total_length - already_sent, 0);
            if (sent < 0)
            {
                fprintf(stderr, "Error: send() failed\n");
                perror("send");
                free(file_data);
                return -1;
            }
            else if (sent == 0)
            {
                fprintf(stderr, "Error: server closed connection\n");
                free(file_data);
                return -1;
            }
            already_sent += sent;
            printf("client downloading %lf%% ...\n", (double)already_sent / total_length * 100);
        }
        printf("client download complete\n");

        free(file_data);
    }
    return 0;
}

int server_put(char *file_name, int sock_fd)
{
    /*
    服务器端组织并发送 put 回复报文
    */
    DATA_STREAM put_reply = {
        .m_protocol = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')},
        .m_type = 0xA8,
        .m_status = 0,
        .m_length = htonl(HEADER_SIZE),
    };

    u_int32_t already_sent = 0, total_length = ntohl(put_reply.m_length);
    while (already_sent < total_length)
    {
        int sent = send(sock_fd, (u_int8_t *)&put_reply + already_sent, total_length - already_sent, 0);
        if (sent < 0)
        {
            fprintf(stderr, "Error: send() failed\n");
            perror("send");
            return -1;
        }
        else if (sent == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            return -1;
        }
        already_sent += sent;
    }

    /*
    服务器端接收 FILE_DATA 响应报文，至少接收完报文头
    */
    DATA_STREAM *file_data = (DATA_STREAM *)malloc(sizeof(DATA_STREAM));
    int already_received = 0, expect_length = HEADER_SIZE;
    while (already_received < expect_length)
    {
        int received = recv(sock_fd, (u_int8_t *)file_data + already_received, expect_length - already_received, 0);
        if (received < 0)
        {
            fprintf(stderr, "Error: recv() failed\n");
            perror("recv");
            free(file_data);
            return -1;
        }
        else if (received == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            free(file_data);
            return -1;
        }
        already_received += received;
        printf("client uploading %lf%% ...\n", (double)already_received / HEADER_SIZE * 100);
    }

    /*
    检查 FILE_DATA 响应报文头是否合法
    */
    if (check_protocol(file_data) == false)
    {
        fprintf(stderr, "Error: file data response protocol number is not correct\n");
        free(file_data);
        return -1;
    }
    if (file_data->m_type != 0xFF)
    {
        fprintf(stderr, "Error: file data response type is not correct\n");
        free(file_data);
        return -1;
    }

    /*
    分配足够大的内存继续接收响应报文体 entity body，使用 realloc() 扩充内存
    */
    file_data = (DATA_STREAM *)realloc(file_data, ntohl(file_data->m_length));
    expect_length = ntohl(file_data->m_length);
    while (already_received < expect_length)
    {
        int received = recv(sock_fd, (u_int8_t *)file_data + already_received, expect_length - already_received, 0);
        if (received < 0)
        {
            fprintf(stderr, "Error: recv() failed\n");
            perror("recv");
            free(file_data);
            return -1;
        }
        else if (received == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            free(file_data);
            return -1;
        }
        already_received += received;
    }

    /*
    写入文件
    */
    FILE *fp = fopen(file_name, "wb");
    if (fp == NULL)
    {
        fprintf(stderr, "Error: fopen() failed\n");
        perror("fopen");
        free(file_data);
        return -1;
    }
    fwrite(file_data->payload, 1, strlen(file_data->payload), fp);
    fclose(fp);
    printf("client upload complete\n");

    free(file_data);
    return 0;
}

int server_sha256(char *file_name, int sock_fd)
{
    if (access(file_name, F_OK) == -1)
    {
        /*
        服务器端组织并发送 sha256 回复报文
        */
        DATA_STREAM get_reply = {
            .m_protocol = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')},
            .m_type = 0xAA,
            .m_status = 0,
            .m_length = htonl(HEADER_SIZE),
        };

        u_int32_t already_sent = 0, total_length = ntohl(get_reply.m_length);
        while (already_sent < total_length)
        {
            int sent = send(sock_fd, (u_int8_t *)&get_reply + already_sent, total_length - already_sent, 0);
            if (sent < 0)
            {
                fprintf(stderr, "Error: send() failed\n");
                perror("send");
                return -1;
            }
            else if (sent == 0)
            {
                fprintf(stderr, "Error: server closed connection\n");
                return -1;
            }
            already_sent += sent;
        }
    }
    else
    {
        /*
        服务器端组织并发送 sha256 回复报文
        */
        DATA_STREAM get_reply = {
            .m_protocol = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')},
            .m_type = 0xAA,
            .m_status = 1,
            .m_length = htonl(HEADER_SIZE),
        };

        int already_sent = 0, total_length = ntohl(get_reply.m_length);
        while (already_sent < total_length)
        {
            int sent = send(sock_fd, (u_int8_t *)&get_reply + already_sent, total_length - already_sent, 0);
            if (sent < 0)
            {
                fprintf(stderr, "Error: send() failed\n");
                perror("send");
                return -1;
            }
            else if (sent == 0)
            {
                fprintf(stderr, "Error: server closed connection\n");
                return -1;
            }
            already_sent += sent;
        }

        /*
        如果文件存在，服务器端计算 sha256 值
        */
        std::string cmd = "sha256sum " + std::string(file_name);
        FILE *fp = popen(cmd.c_str(), "r"); // 流文件，不知道大小
        if (fp == NULL)
        {
            fprintf(stderr, "Error: popen() failed\n");
            perror("popen");
            return -1;
        }
        char buffer[BUF_SIZE];
        int ls_size = fread(buffer, 1, BUF_SIZE, fp);
        if (ls_size < 0)
        {
            fprintf(stderr, "Error: fread() failed\n");
            perror("fread");
            return -1;
        }
        pclose(fp);

        /*
        服务器端组织并发送 sha256 数据报文
        */
        DATA_STREAM *file_data = (DATA_STREAM *)malloc(HEADER_SIZE + ls_size + 1);
        file_data->m_protocol[0] = u_int8_t(0xc1), file_data->m_protocol[1] = u_int8_t(0xa1), file_data->m_protocol[2] = u_int8_t(0x10), file_data->m_protocol[3] = u_int8_t('f'), file_data->m_protocol[4] = u_int8_t('t'), file_data->m_protocol[5] = u_int8_t('p');
        file_data->m_type = 0xFF;
        file_data->m_status = 0;
        file_data->m_length = htonl(HEADER_SIZE + ls_size + 1);
        memcpy(file_data->payload, buffer, ls_size);

        already_sent = 0, total_length = ntohl(file_data->m_length);
        while (already_sent < total_length)
        {
            int sent = send(sock_fd, (u_int8_t *)file_data + already_sent, total_length - already_sent, 0);
            if (sent < 0)
            {
                fprintf(stderr, "Error: send() failed\n");
                perror("send");
                free(file_data);
                return -1;
            }
            else if (sent == 0)
            {
                fprintf(stderr, "Error: server closed connection\n");
                free(file_data);
                return -1;
            }
            already_sent += sent;
            printf("transmitting %lf%% ...\n", (double)already_sent / total_length * 100);
        }
        printf("client sha256 complete\n");

        free(file_data);
    }
    return 0;
}

int server_quit(int sock_fd)
{
    /*
    服务器端组织并发送 quit 回复报文
    */
    DATA_STREAM quit_reply = {
        .m_protocol = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')},
        .m_type = 0xAC,
        .m_status = 0,
        .m_length = htonl(HEADER_SIZE),
    };

    u_int32_t already_sent = 0, total_length = ntohl(quit_reply.m_length);
    while (already_sent < total_length)
    {
        int sent = send(sock_fd, (u_int8_t *)&quit_reply + already_sent, total_length - already_sent, 0);
        if (sent < 0)
        {
            fprintf(stderr, "Error: send() failed\n");
            perror("send");
            return -1;
        }
        else if (sent == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            return -1;
        }
        already_sent += sent;
    }

    /*
    关闭连接并更新全局变量
    */
    if (close(sock_fd) < 0)
    {
        fprintf(stderr, "Error: close() failed\n");
        perror("close");
        return -1;
    }
    status = FTP_SERVER_STATUS::IDLE;
    return 0;
}