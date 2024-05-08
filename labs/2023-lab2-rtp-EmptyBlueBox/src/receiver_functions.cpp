/**
 * @file receiver_functions.c
 * @brief Implementation of the functions in receiver_functions.h
 * @note The functions are used in receiver.c
 * @author Yutong LIANG
 * @date 11/4/2023
 */

#include "receiver_functions.h"
#include "rtp.h"
#include "util.h"
#include <arpa/inet.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <deque>
#include <errno.h>

int receive_fd;
struct sockaddr_in receiver_addr;
struct sockaddr_in sender_addr;
socklen_t sender_addr_len;
FILE *fp;

uint32_t next_seq_num = 0;

int init_receiver(char *listen_port, char *file_path)
{
    LOG_DEBUG("Receiver: into init_receiver()\n");

    // 初始化接收端
    receive_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (receive_fd < 0)
        LOG_SYS_FATAL("socket() in init_receiver() failed\n");
    memset(&receiver_addr, 0, sizeof(receiver_addr));                                     // receiver_addr is the address on which the receiver listens
    receiver_addr.sin_family = AF_INET;                                                   // IPv4
    receiver_addr.sin_addr.s_addr = htonl(INADDR_ANY);                                    // receiver accepts incoming data from any address
    receiver_addr.sin_port = htons(atoi(listen_port));                                    // but only accepts data coming from a certain port
    if (bind(receive_fd, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) == -1) // assign the address to the listening socket
        LOG_SYS_FATAL("bind() in init_receiver() failed\n");

    // 设置接收超时，如果 5s 收不到报文就说明连接已断开，对全局传输过程均成立
    // 如果超时, recv 返回 -1
    struct timeval tv;
    tv.tv_sec = 5; // 设置超时时间为 5 秒
    tv.tv_usec = 0;
    if (setsockopt(receive_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        LOG_SYS_FATAL("setsockopt() in receiver_establish_connection() failed\n");

    // 打开文件
    fp = fopen(file_path, "wb");
    if (fp == NULL)
        LOG_SYS_FATAL("fopen() in init_receiver() failed\n");

    LOG_DEBUG("Receiver: exit init_receiver()\n");

    return 0;
}

int receiver_establish_connection()
{
    LOG_DEBUG("Receiver: into receiver_establish_connection()\n");

    sender_addr_len = sizeof(sender_addr);

    clock_t start, end;
    // 接收第一次握手报文
    rtp_packet_t *start_pkt;
    while (1)
    {
        start_pkt = rtp_recvfrom(receive_fd, (struct sockaddr *)&sender_addr, &sender_addr_len);
        // 如果收到的是不合法的报文，那么就丢弃这个包
        if (start_pkt == NULL)
            continue;
        // 如果收到的报文不是第一次握手报文，那么就丢弃这个包
        else if (start_pkt->rtp.flags != RTP_SYN)
        {
            free(start_pkt);
            continue;
        }
        // 如果收到的报文是第一次握手报文，那么就发送第二次握手报文
        else if (start_pkt->rtp.flags == RTP_SYN)
        {
            next_seq_num = start_pkt->rtp.seq_num + 1;
            rtp_sendto(receive_fd, NULL, next_seq_num, 0, RTP_SYN | RTP_ACK, (struct sockaddr *)&sender_addr, sender_addr_len);
            start = clock();
            free(start_pkt);
            break;
        }
    }

    // 接收第三次握手报文
    rtp_packet_t *ack_pkt;
    while (1)
    {
        end = clock(); // 终止时间要在接收报文之前，否则可能会出现实际上没有超时，但是判定为超时的情况
        ack_pkt = rtp_recvfrom(receive_fd, (struct sockaddr *)&sender_addr, &sender_addr_len);
        // 如果收到的是不合法的报文，那么就丢弃这个包
        if (ack_pkt == NULL)
            continue;
        // 如果第二次握手报文 ack 超时，那么就重新发送第二次握手报文
        // 如果收到第一次握手报文，因为第一次握手报文可能是重传的，那么就重新发送第二次握手报文
        else if ((double)(end - start) / CLOCKS_PER_SEC > PACKET_TIMEOUT_SEC || (ack_pkt->rtp.seq_num == next_seq_num - 1 && ack_pkt->rtp.flags == RTP_SYN))
        {
            rtp_sendto(receive_fd, NULL, next_seq_num, 0, RTP_SYN | RTP_ACK, (struct sockaddr *)&sender_addr, sender_addr_len);
            start = clock();
            continue;
        }
        // 如果收到的报文不是第三次握手报文，那么就丢弃这个包，因为已经判断了是不是第一次握手报文， sender 没有别的重传包了
        else if (ack_pkt->rtp.seq_num != next_seq_num || ack_pkt->rtp.flags != RTP_ACK)
        {
            free(ack_pkt);
            continue;
        }
        // 如果收到的报文是第三次握手报文，那么建立连接
        else if (ack_pkt->rtp.seq_num == next_seq_num && ack_pkt->rtp.flags == RTP_ACK)
        {
            // next_seq_num++; // 成功连接之后，Sender发送的第一个数据报文（flag为0）的seq_num应该为x+1（假设SYN报文的编号为x）
            free(ack_pkt);
            break;
        }
    }

    LOG_DEBUG("Receiver: exit receiver_establish_connection(), connection established!\n");

    return 0;
}

int receive_data_GBN(char *window_size_str)
{
    LOG_DEBUG("Receiver: into receive_data_GBN(), window size = %s\n", window_size_str);

    rtp_packet_t *pkt;

#ifdef LDEBUG
    int loop_count = 1;
#endif
    while (1)
    {
        LOG_DEBUG("\n");
        LOG_DEBUG("Receiver: loop_count = %d\n", loop_count++);

        pkt = rtp_recvfrom(receive_fd, (struct sockaddr *)&sender_addr, &sender_addr_len);
        // 如果收到的是不合法的报文，那么就丢弃这个包
        if (pkt == NULL)
            continue;
        // 如果收到了 FIN 报文，那么说明 sender 想终止连接，receiver 退出循环
        else if (pkt->rtp.seq_num == next_seq_num && pkt->rtp.flags == RTP_FIN)
        {
            free(pkt);
            break;
        }
        // 如果收到了期望的报文，那么就写入文件，发送 ACK 报文，然后期望的报文序号加一
        else if (pkt->rtp.seq_num == next_seq_num && pkt->rtp.flags == 0)
        {
            fwrite(pkt->payload, 1, pkt->rtp.length, fp);
            next_seq_num++;
            rtp_sendto(receive_fd, NULL, next_seq_num, 0, RTP_ACK, (struct sockaddr *)&sender_addr, sender_addr_len);
            LOG_DEBUG("Receiver: send ACK: seq_num = %d\n", next_seq_num);
            free(pkt);
        }
        // 如果收到了不期望的报文，那么就发送 ACK 报文，然后继续等待
        else if (pkt->rtp.seq_num != next_seq_num && pkt->rtp.flags == 0)
        {
            LOG_DEBUG("unexpected packet\n");
            rtp_sendto(receive_fd, NULL, next_seq_num, 0, RTP_ACK, (struct sockaddr *)&sender_addr, sender_addr_len);
            free(pkt);
        }
    }

    LOG_DEBUG("Receiver: exit receive_data_GBN()\n");
    return 0;
}

int receive_data_SR(char *window_size_str)
{
    LOG_DEBUG("Receiver: into receive_data_SR(), window size = %s\n", window_size_str);

    rtp_packet_t *pkt;
    int window_size = atoi(window_size_str);
    std::deque<rtp_packet_t *> window(window_size, NULL);

    while (1)
    {
        pkt = rtp_recvfrom(receive_fd, (struct sockaddr *)&sender_addr, &sender_addr_len);
        // 如果收到的是不合法的报文，那么就丢弃这个包
        if (pkt == NULL)
            continue;
        // 如果收到了 FIN 报文，那么说明 sender 想终止连接，receiver 退出循环
        else if (pkt->rtp.seq_num == next_seq_num && pkt->rtp.flags == RTP_FIN)
        {
            free(pkt);
            break;
        }
        // 如果收到了可以移动 window 的期望的报文，那么就写入文件直到遇见空指针为止（相当于移动window），发送 ACK 报文，然后期望的报文序号加一
        else if (pkt->rtp.seq_num == next_seq_num && pkt->rtp.flags == 0)
        {
            LOG_DEBUG("Receiver: receive data: seq_num = %d, length = %d, payload = %s\n", pkt->rtp.seq_num, pkt->rtp.length, pkt->payload);
            window[0] = pkt;
            while (window.front() != NULL)
            {
                fwrite(window[0]->payload, 1, window[0]->rtp.length, fp);
                free(window.front());
                window.pop_front();
                window.push_back(NULL);
                rtp_sendto(receive_fd, NULL, next_seq_num, 0, RTP_ACK, (struct sockaddr *)&sender_addr, sender_addr_len);
                next_seq_num++;
            }
        }
        // 如果收到了 window 内但是不是第一个的报文，那么就发送 ACK 报文
        else if (next_seq_num < pkt->rtp.seq_num && pkt->rtp.seq_num <= next_seq_num + window_size - 1 && pkt->rtp.flags == 0)
        {
            window[pkt->rtp.seq_num - next_seq_num] = pkt;
            rtp_sendto(receive_fd, NULL, pkt->rtp.seq_num, 0, RTP_ACK, (struct sockaddr *)&sender_addr, sender_addr_len);
        }
        // 如果收到了 window 前一个 window 的报文，那么就发送 ACK 报文
        else if (next_seq_num - window_size <= pkt->rtp.seq_num && pkt->rtp.seq_num <= next_seq_num - 1 && pkt->rtp.flags == 0)
        {
            rtp_sendto(receive_fd, NULL, pkt->rtp.seq_num, 0, RTP_ACK, (struct sockaddr *)&sender_addr, sender_addr_len);
            free(pkt);
        }
    }

    LOG_DEBUG("Receiver: exit receive_data_SR()\n");
    return 0;
}

int receiver_terminate_connection()
{
    LOG_DEBUG("Receiver: into receiver_terminate_connection()\n");

    rtp_sendto(receive_fd, NULL, next_seq_num, 0, RTP_FIN | RTP_ACK, (struct sockaddr *)&sender_addr, sender_addr_len);

    while (1)
    {
        errno = 0;
        rtp_packet_t *pkt = rtp_recvfrom(receive_fd, (struct sockaddr *)&sender_addr, &sender_addr_len, false);
        // 如果 rtp_recvfrom() 超时，那么说明 sender 的连接已经断开，退出循环
        if (pkt == NULL && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        // 如果收到的是不合法的报文，那么就丢弃这个包
        else if (pkt == NULL)
            continue;
        // 如果收到报文，就重复发送FIN|ACK报文，直到 rtp_recvfrom() 超时
        else
        {
            rtp_sendto(receive_fd, NULL, next_seq_num, 0, RTP_FIN | RTP_ACK, (struct sockaddr *)&sender_addr, sender_addr_len);
            free(pkt);
        }
    }

    LOG_DEBUG("Receiver: exit receiver_terminate_connection()\n");
    return 0;
}

int delete_receiver()
{
    LOG_DEBUG("Receiver: into delete_receiver()\n");

    close(receive_fd);
    fclose(fp);

    LOG_DEBUG("Receiver: exit delete_receiver()\n");
    return 0;
}