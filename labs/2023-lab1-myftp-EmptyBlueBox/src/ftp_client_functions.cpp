#include "ftp_client_functions.h"
#include "ftp_client_utils.h"
#include <string>
#include <sstream>
#include <vector>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

int client_open(char *command, int sock_fd)
{
    if (status == FTP_CLIENT_STATUS::CONNECTED)
    {
        fprintf(stderr, "Error: already connected to %s\n", connecting.c_str());
        return -1;
    }

    std::string command_str(command);
    std::stringstream ss(command_str);
    std::string token;
    std::vector<std::string> args;

    while (ss >> token)
        args.push_back(token);
    if (args.size() != 3)
    {
        fprintf(stderr, "Error: open command should have 2 arguments\n");
        return -1;
    }

    /*
    客户端组织服务器的地址信息
    */
    std::string ip = args[1];
    int port = std::stoi(args[2].c_str());
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) < 0)
    {
        fprintf(stderr, "Error: inet_pton() failed\n");
        perror("inet_pton");
        return -1;
    }

    /*
    客户端调用connect()函数主动发起连接请求
    */
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "Error: connect() failed\n");
        perror("connect");
        return -1;
    }

    /*
    客户端组织并发送open请求报文，使用 Designated Initializers 初始化
    */
    DATA_STREAM open_request = {
        .m_protocol = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')},
        .m_type = 0xA1,
        .m_status = 0,
        .m_length = htonl(HEADER_SIZE),
    };

    u_int32_t already_sent = 0, total_length = ntohl(open_request.m_length);
    while (already_sent < total_length)
    {
        int sent = send(sock_fd, (u_int8_t *)&open_request + already_sent, total_length - already_sent, 0);
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
    客户端接收并解析open响应报文
    */
    DATA_STREAM open_reply;
    u_int32_t already_received = 0, expect_length = HEADER_SIZE;
    while (already_received < expect_length)
    {
        int received = recv(sock_fd, (u_int8_t *)&open_reply + already_received, expect_length - already_received, 0);
        if (received < 0)
        {
            fprintf(stderr, "Error: recv() failed\n");
            perror("recv");
            return -1;
        }
        else if (received == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            return -1;
        }
        already_received += received;
    }

    /*
    检查open响应报文是否合法
    */
    if (check_protocol(&open_reply) == false)
    {
        fprintf(stderr, "Error: open response protocol number is not correct\n");
        return -1;
    }
    if (open_reply.m_type != 0xA2)
    {
        fprintf(stderr, "Error: open response type is not correct\n");
        return -1;
    }
    if (open_reply.m_status != 1)
    {
        fprintf(stderr, "Error: open response status is not correct\n");
        return -1;
    }

    /*
    更新全局变量
    */
    connecting = get_ip_port(command);
    status = FTP_CLIENT_STATUS::CONNECTED;

    return 0;
}

int client_ls(int sock_fd)
{
    if (status == FTP_CLIENT_STATUS::IDLE)
    {
        fprintf(stderr, "Error: not connected\n");
        return -1;
    }

    /*
    客户端组织并发送 ls 请求报文，使用 Designated Initializers 初始化
    */
    DATA_STREAM ls_request = {
        .m_protocol = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')},
        .m_type = 0xA3,
        .m_status = 0,
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

    /*
    客户端接收 ls 响应报文，至少接收完报文头
    */
    DATA_STREAM *ls_reply = (DATA_STREAM *)malloc(sizeof(DATA_STREAM));
    u_int32_t already_received = 0, expect_length = HEADER_SIZE;
    while (already_received < expect_length)
    {
        int received = recv(sock_fd, (u_int8_t *)ls_reply + already_received, expect_length - already_received, 0);
        if (received < 0)
        {
            fprintf(stderr, "Error: recv() failed\n");
            perror("recv");
            free(ls_reply);
            return -1;
        }
        else if (received == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            free(ls_reply);
            return -1;
        }
        already_received += received;
    }

    /*
    检查 ls 响应报文头是否合法
    */
    if (check_protocol(ls_reply) == false)
    {
        fprintf(stderr, "Error: open response protocol number is not correct\n");
        free(ls_reply);
        return -1;
    }
    if (ls_reply->m_type != 0xA4)
    {
        fprintf(stderr, "Error: ls response type is not correct\n");
        free(ls_reply);
        return -1;
    }

    /*
    分配足够大的内存继续接收响应报文体 entity body，使用 realloc() 扩充内存
    */
    ls_reply = (DATA_STREAM *)realloc(ls_reply, ntohl(ls_reply->m_length));
    expect_length = ntohl(ls_reply->m_length);
    while (already_received < expect_length)
    {
        int received = recv(sock_fd, (u_int8_t *)ls_reply + already_received, expect_length - already_received, 0);
        if (received < 0)
        {
            fprintf(stderr, "Error: recv() failed\n");
            perror("recv");
            free(ls_reply);
            return -1;
        }
        else if (received == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            free(ls_reply);
            return -1;
        }
        already_received += received;
    }

    printf("-----file list start-----\n");
    printf("%s", ls_reply->payload);
    printf("-----file list end-----\n");

    free(ls_reply);
    return 0;
}

int client_get(char *command, int sock_fd)
{
    if (status == FTP_CLIENT_STATUS::IDLE)
    {
        fprintf(stderr, "Error: not connected\n");
        return -1;
    }

    std::string command_str(command);
    std::stringstream ss(command_str);
    std::string token;
    std::vector<std::string> args;

    while (ss >> token)
        args.push_back(token);
    if (args.size() != 2)
    {
        fprintf(stderr, "Error: get command should have 1 argument\n");
        return -1;
    }

    /*
    客户端组织并发送 get 请求报文
    */
    DATA_STREAM *get_request = (DATA_STREAM *)malloc(HEADER_SIZE + args[1].length() + 1);
    get_request->m_protocol[0] = u_int8_t(0xc1), get_request->m_protocol[1] = u_int8_t(0xa1), get_request->m_protocol[2] = u_int8_t(0x10), get_request->m_protocol[3] = u_int8_t('f'), get_request->m_protocol[4] = u_int8_t('t'), get_request->m_protocol[5] = u_int8_t('p');
    get_request->m_type = 0xA5;
    get_request->m_status = 0;
    get_request->m_length = htonl(HEADER_SIZE + args[1].length() + 1);
    memcpy(get_request->payload, args[1].c_str(), args[1].length() + 1);

    u_int32_t already_sent = 0, total_length = ntohl(get_request->m_length);
    while (already_sent < total_length)
    {
        int sent = send(sock_fd, (u_int8_t *)get_request + already_sent, total_length - already_sent, 0);
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
    客户端接收并解析 get 响应报文
    */
    DATA_STREAM get_reply;
    u_int32_t already_received = 0, expect_length = HEADER_SIZE;
    while (already_received < expect_length)
    {
        int received = recv(sock_fd, (u_int8_t *)&get_reply + already_received, expect_length - already_received, 0);
        if (received < 0)
        {
            fprintf(stderr, "Error: recv() failed\n");
            perror("recv");
            return -1;
        }
        else if (received == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            return -1;
        }
        already_received += received;
    }

    /*
    检查 get 响应报文是否合法
    */
    if (check_protocol(&get_reply) == false)
    {
        fprintf(stderr, "Error: get response protocol number is not correct\n");
        return -1;
    }
    if (get_reply.m_type != 0xA6)
    {
        fprintf(stderr, "Error: get response type is not correct\n");
        return -1;
    }
    if (get_reply.m_status == 0)
    {
        fprintf(stderr, "Error: file not found\n");
        return -1;
    }
    else if (get_reply.m_status != 1)
    {
        fprintf(stderr, "Error: get response status is not correct\n");
        return -1;
    }

    /*
    如果文件存在，客户端接收并保存文件
    客户端接收 FILE_DATA 响应报文，至少接收完报文头
    */
    DATA_STREAM *file_data = (DATA_STREAM *)malloc(sizeof(DATA_STREAM));
    already_received = 0, expect_length = HEADER_SIZE;
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
        printf("downloading %lf%% ...\n", (double)already_received / HEADER_SIZE * 100);
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
    FILE *fp = fopen(args[1].c_str(), "wb");
    if (fp == NULL)
    {
        fprintf(stderr, "Error: fopen() failed\n");
        perror("fopen");
        free(file_data);
        return -1;
    }
    fwrite(file_data->payload, 1, strlen(file_data->payload), fp);
    fclose(fp);
    printf("download complete\n");

    free(file_data);
    return 0;
}

int client_put(char *command, int sock_fd)
{
    if (status == FTP_CLIENT_STATUS::IDLE)
    {
        fprintf(stderr, "Error: not connected\n");
        return -1;
    }

    std::string command_str(command);
    std::stringstream ss(command_str);
    std::string token;
    std::vector<std::string> args;

    while (ss >> token)
        args.push_back(token);
    if (args.size() != 2)
    {
        fprintf(stderr, "Error: put command should have 1 argument\n");
        return -1;
    }
    /*
    客户端检查文件是否存在
    */
    if (access(args[1].c_str(), F_OK))
    {
        fprintf(stderr, "Error: file not found\n");
        return -1;
    }

    /*
    客户端组织并发送 put 请求报文
    */
    DATA_STREAM *put_request = (DATA_STREAM *)malloc(HEADER_SIZE + args[1].length() + 1);
    put_request->m_protocol[0] = u_int8_t(0xc1), put_request->m_protocol[1] = u_int8_t(0xa1), put_request->m_protocol[2] = u_int8_t(0x10), put_request->m_protocol[3] = u_int8_t('f'), put_request->m_protocol[4] = u_int8_t('t'), put_request->m_protocol[5] = u_int8_t('p');
    put_request->m_type = 0xA7;
    put_request->m_status = 0;
    put_request->m_length = htonl(HEADER_SIZE + args[1].length() + 1);
    memcpy(put_request->payload, args[1].c_str(), args[1].length() + 1);

    u_int32_t already_sent = 0, total_length = ntohl(put_request->m_length);
    while (already_sent < total_length)
    {
        int sent = send(sock_fd, (u_int8_t *)put_request + already_sent, total_length - already_sent, 0);
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
    客户端接收并解析 put 响应报文
    */
    DATA_STREAM put_reply;
    u_int32_t already_received = 0, expect_length = HEADER_SIZE;
    while (already_received < expect_length)
    {
        int received = recv(sock_fd, (u_int8_t *)&put_reply + already_received, expect_length - already_received, 0);
        if (received < 0)
        {
            fprintf(stderr, "Error: recv() failed\n");
            perror("recv");
            return -1;
        }
        else if (received == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            return -1;
        }
        already_received += received;
    }

    /*
    检查 put 响应报文是否合法
    */
    if (check_protocol(&put_reply) == false)
    {
        fprintf(stderr, "Error: put response protocol number is not correct\n");
        return -1;
    }
    if (put_reply.m_type != 0xA8)
    {
        fprintf(stderr, "Error: put response type is not correct\n");
        return -1;
    }

    /*
    如果文件存在，客户端组织文件数据
    */
    FILE *fp = fopen(args[1].c_str(), "rb");
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
    客户端发送文件
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
        printf("uploading %lf%% ...\n", (double)already_sent / total_length * 100);
    }
    printf("upload complete\n");

    free(file_data);
    return 0;
}

int client_sha256(char *command, int sock_fd)
{
    if (status == FTP_CLIENT_STATUS::IDLE)
    {
        fprintf(stderr, "Error: not connected\n");
        return -1;
    }

    std::string command_str(command);
    std::stringstream ss(command_str);
    std::string token;
    std::vector<std::string> args;

    while (ss >> token)
        args.push_back(token);
    if (args.size() != 2)
    {
        fprintf(stderr, "Error: sha256 command should have 1 argument\n");
        return -1;
    }

    /*
    客户端组织并发送 sha256 请求报文
    */
    DATA_STREAM *sha256_request = (DATA_STREAM *)malloc(HEADER_SIZE + args[1].length() + 1);
    sha256_request->m_protocol[0] = u_int8_t(0xc1), sha256_request->m_protocol[1] = u_int8_t(0xa1), sha256_request->m_protocol[2] = u_int8_t(0x10), sha256_request->m_protocol[3] = u_int8_t('f'), sha256_request->m_protocol[4] = u_int8_t('t'), sha256_request->m_protocol[5] = u_int8_t('p');
    sha256_request->m_type = 0xA9;
    sha256_request->m_status = 0;
    sha256_request->m_length = htonl(HEADER_SIZE + args[1].length() + 1);
    memcpy(sha256_request->payload, args[1].c_str(), args[1].length() + 1);

    u_int32_t already_sent = 0, total_length = ntohl(sha256_request->m_length);
    while (already_sent < total_length)
    {
        int sent = send(sock_fd, (u_int8_t *)sha256_request + already_sent, total_length - already_sent, 0);
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
    客户端接收并解析 sha256 响应报文
    */
    DATA_STREAM sha256_reply;
    u_int32_t already_received = 0, expect_length = HEADER_SIZE;
    while (already_received < expect_length)
    {
        int received = recv(sock_fd, (u_int8_t *)&sha256_reply + already_received, expect_length - already_received, 0);
        if (received < 0)
        {
            fprintf(stderr, "Error: recv() failed\n");
            perror("recv");
            return -1;
        }
        else if (received == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            return -1;
        }
        already_received += received;
    }

    /*
    检查 sha256 响应报文是否合法
    */
    if (check_protocol(&sha256_reply) == false)
    {
        fprintf(stderr, "Error: sha256 response protocol number is not correct\n");
        return -1;
    }
    if (sha256_reply.m_type != 0xAA)
    {
        fprintf(stderr, "Error: sha256 response type is not correct\n");
        return -1;
    }
    if (sha256_reply.m_status == 0)
    {
        fprintf(stderr, "Error: file not found\n");
        return -1;
    }
    else if (sha256_reply.m_status != 1)
    {
        fprintf(stderr, "Error: get response status is not correct\n");
        return -1;
    }

    /*
    如果文件存在，客户端接收并保存文件的sha256
    客户端接收 FILE_DATA 响应报文，至少接收完报文头
    */
    DATA_STREAM *file_data = (DATA_STREAM *)malloc(sizeof(DATA_STREAM));
    already_received = 0, expect_length = HEADER_SIZE;
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
    检查 FILE_DATA 响应报文头是否合法
    */
    if (check_protocol(file_data) == false)
    {
        fprintf(stderr, "Error: file data protocol number is not correct\n");
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

    printf("-----sha256 start-----\n");
    printf("%s", file_data->payload);
    printf("-----sha256 end-----\n");

    free(file_data);
    return 0;
}

int client_quit(int sock_fd)
{
    if (status == FTP_CLIENT_STATUS::IDLE)
    {
        exit(0);
    }

    /*
    客户端组织并发送 quit 请求报文，使用 Designated Initializers 初始化
    */
    DATA_STREAM quit_request = {
        .m_protocol = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')},
        .m_type = 0xAB,
        .m_status = 0,
        .m_length = htonl(HEADER_SIZE),
    };

    u_int32_t already_sent = 0, total_length = ntohl(quit_request.m_length);
    while (already_sent < total_length)
    {
        int sent = send(sock_fd, (u_int8_t *)&quit_request + already_sent, total_length - already_sent, 0);
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
    客户端接收并解析 quit 响应报文
    */
    DATA_STREAM quit_reply;
    u_int32_t already_received = 0, expect_length = HEADER_SIZE;
    while (already_received < expect_length)
    {
        int received = recv(sock_fd, (u_int8_t *)&quit_reply + already_received, expect_length - already_received, 0);
        if (received < 0)
        {
            fprintf(stderr, "Error: recv() failed\n");
            perror("recv");
            return -1;
        }
        else if (received == 0)
        {
            fprintf(stderr, "Error: server closed connection\n");
            return -1;
        }
        already_received += received;
    }

    /*
    检查 quit 响应报文是否合法
    */
    if (check_protocol(&quit_reply) == false)
    {
        fprintf(stderr, "Error: quit response protocol number is not correct\n");
        return -1;
    }
    if (quit_reply.m_type != 0xAC)
    {
        fprintf(stderr, "Error: quit response type is not correct\n");
        return -1;
    }

    /*
    关闭套接字，更新全局变量
    */
    if (close(sock_fd) < 0)
    {
        fprintf(stderr, "Error: close() failed\n");
        perror("close");
        return -1;
    }
    connecting = "none";
    status = FTP_CLIENT_STATUS::IDLE;
    printf("quit success\n");
    return 0;
}