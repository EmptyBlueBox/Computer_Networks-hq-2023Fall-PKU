#include "sender_functions.h"
#include "rtp.h"
#include "util.h"
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <set>

int send_fd;
struct sockaddr_in receiver_addr;
socklen_t receiver_addr_len = sizeof(receiver_addr);

FILE *fp;
uint32_t file_size;
uint32_t max_packet_num;

uint32_t send_base = 1;    // 三次握手不改变 send_base
uint32_t next_seq_num = 0; // 三次握手后，next_seq_num = 1 ，恰好是要发的下一个数据包，也即第一个数据包的序号

int init_sender(char *remote_IP, char *remote_port, char *file_path)
{
    LOG_DEBUG("Sender: into init_sender()\n");

    // 初始化发送端
    send_fd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&receiver_addr, 0, sizeof(receiver_addr)); // receiver_addr is the receiver's location in the network
    receiver_addr.sin_family = AF_INET;
    inet_pton(AF_INET, remote_IP, &receiver_addr.sin_addr);
    receiver_addr.sin_port = htons(atoi(remote_port));
    LOG_DEBUG("address: %s:%d\n", inet_ntoa(receiver_addr.sin_addr), ntohs(receiver_addr.sin_port));

    // 打开文件
    fp = fopen(file_path, "rb");
    if (fp == NULL)
        LOG_SYS_FATAL("fopen() in init_sender() failed\n");

    // 计算文件总大小
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    max_packet_num = file_size / PAYLOAD_MAX + (file_size % PAYLOAD_MAX == 0 ? 0 : 1);
    LOG_DEBUG("file size: %d, max packet num: %d\n", file_size, max_packet_num);

    LOG_DEBUG("Sender: exit init_sender()\n");
    return 0;
}

int sender_establish_connection()
{
    LOG_DEBUG("\n");
    LOG_DEBUG("Sender: into sender_establish_connection()\n");

    struct timeval start, end;

    // 发送第一次握手报文
    LOG_DEBUG("Sender: send the first handshake packet\n");
    rtp_sendto(send_fd, NULL, next_seq_num, 0, RTP_SYN, (struct sockaddr *)&receiver_addr, receiver_addr_len);
    gettimeofday(&start, NULL);
    next_seq_num++;

    // 设置接收超时，如果 0.1s 收不到报文就说明 receiver 没能收到第一次握手报文，重传
    // 其实希望使用 clock 就可以处理超时情况，但是可能出现 receiver 不传输报文情况，就需要设置套接字超时来避免一直堵塞
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 设置超时时间为 0.1 秒
    if (setsockopt(send_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        LOG_SYS_FATAL("setsockopt() in sender_establish_connection() failed, failed to set recv timeout\n");

    // 接收第二次握手报文
    LOG_DEBUG("\n");
    LOG_DEBUG("Sender: receive the second handshake packet\n");
#ifdef LDEBUG
    int loop_count = 1;
#endif
    while (1)
    {
        LOG_DEBUG("\n");
        LOG_DEBUG("Sender: loop_count: %d\n", loop_count++);
        errno = 0;
        gettimeofday(&end, NULL);
        rtp_packet_t *packet = rtp_recvfrom(send_fd, (struct sockaddr *)&receiver_addr, &receiver_addr_len, false);
        // 如果第二次握手报文的 ACK 超时（套接字超时），重传
        if (packet == NULL && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            LOG_DEBUG("Sender: resend the first handshake packet\n");
            rtp_sendto(send_fd, NULL, next_seq_num - 1, 0, RTP_SYN, (struct sockaddr *)&receiver_addr, receiver_addr_len);
            gettimeofday(&start, NULL);
        }
        // 如果第二次握手报文的 ACK 超时（普通超时），重传
        else if (get_time_interval_by_second(&start, &end) > PACKET_TIMEOUT_SEC)
        {
            LOG_DEBUG("Sender: resend the first handshake packet\n");
            rtp_sendto(send_fd, NULL, next_seq_num - 1, 0, RTP_SYN, (struct sockaddr *)&receiver_addr, receiver_addr_len);
            gettimeofday(&start, NULL);
            free(packet);
        }
        // 如果收到的是不合法的报文，丢弃
        else if (packet == NULL)
            continue;
        // 收到不用回复的报文，丢弃
        else if (packet->rtp.seq_num != next_seq_num || packet->rtp.flags != (RTP_SYN | RTP_ACK))
        {
            free(packet);
            continue;
        }
        // 收到正确的第二次握手报文，发送第三次握手报文，退出
        else if (packet->rtp.seq_num == next_seq_num && packet->rtp.flags == (RTP_SYN | RTP_ACK))
        {
            LOG_DEBUG("\n");
            LOG_DEBUG("Sender: received the second handshake packet, send the third handshake packet\n");
            rtp_sendto(send_fd, NULL, next_seq_num, 0, RTP_ACK, (struct sockaddr *)&receiver_addr, receiver_addr_len);
            gettimeofday(&start, NULL);
            free(packet);
            break;
        }
        else
        {
            LOG_DEBUG("next_seq_num: %d\n", next_seq_num);
            LOG_DEBUG("packet seq num: %d, packet flags: %d\n", packet->rtp.seq_num, packet->rtp.flags);
            LOG_FATAL("Sender: sender_establish_connection() failed, unexpected situation when trying to receive the second handshake packet\n");
        }
    }

    // 设置接收超时，如果 2s 收不到报文就说明 receiver 已经建立连接
    tv.tv_sec = 2; // 设置超时时间为 2 秒
    tv.tv_usec = 0;
    if (setsockopt(send_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        LOG_SYS_FATAL("setsockopt() in sender_establish_connection() failed, failed to set recv timeout\n");

    // rtp_sendto(send_fd, NULL, next_seq_num - 1, 0, RTP_SYN, (struct sockaddr *)&receiver_addr, receiver_addr_len);

    // 接收发送第三次握手报文后是否还有 SYNACK 数据包，如果没有就可以认为已经建立连接，否则重传第三次握手报文
    LOG_DEBUG("\n");
    LOG_DEBUG("Sender: try to receive packet after the third handshake packet\n");
#ifdef LDEBUG
    loop_count = 1;
#endif
    while (1)
    {
        LOG_DEBUG("\n");
        LOG_DEBUG("Sender: loop_count: %d\n", loop_count++);
        errno = 0;
        gettimeofday(&end, NULL);
        rtp_packet_t *packet = rtp_recvfrom(send_fd, (struct sockaddr *)&receiver_addr, &receiver_addr_len, false);
        // rtp_recvfrom 由于 receiver 建立了连接而超时，说明已经建立连接，退出
        if (packet == NULL && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            LOG_DEBUG("Sender: receiver has established connection!\n");
            break;
        }
        // 计时器超时，说明 receiver 没有建立连接，重传第三次握手报文
        else if (get_time_interval_by_second(&start, &end) > 2)
        {
            LOG_DEBUG("Sender: resend the third handshake packet\n");
            rtp_sendto(send_fd, NULL, next_seq_num, 0, RTP_ACK, (struct sockaddr *)&receiver_addr, receiver_addr_len);
            gettimeofday(&start, NULL);
            free(packet);
            continue;
        }
        // 错误包，丢弃
        else if (packet == NULL)
        {
            LOG_DEBUG("rtp_recvfrom() in sender_establish_connection() failed\n");
            continue;
        }
        // 收到不用回复的报文，丢弃
        else if (packet->rtp.seq_num != next_seq_num || packet->rtp.flags != (RTP_SYN | RTP_ACK))
        {
            free(packet);
            continue;
        }
        // 仍然收到第二次握手报文，重传第三次握手报文
        else if (packet->rtp.seq_num == next_seq_num && packet->rtp.flags == (RTP_SYN | RTP_ACK))
        {
            LOG_DEBUG("Sender: received the second handshake packet again, resend the third handshake packet\n");
            rtp_sendto(send_fd, NULL, next_seq_num, 0, RTP_ACK, (struct sockaddr *)&receiver_addr, receiver_addr_len);
            gettimeofday(&start, NULL);
            free(packet);
            continue;
        }
        else
        {
            LOG_DEBUG("next_seq_num: %d\n", next_seq_num);
            LOG_DEBUG("packet seq num: %d, packet flags: %d\n", packet->rtp.seq_num, packet->rtp.flags);
            LOG_FATAL("Sender: sender_establish_connection() failed, unexpected situation when checking whether the receiver has established a connection\n");
        }
    }

    tv.tv_sec = 0;  // 秒数设置为0
    tv.tv_usec = 0; // 微秒数也设置为0
    // 将接收超时设置为0（取消超时）
    if (setsockopt(send_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0)
        LOG_SYS_FATAL("setsockopt() in sender_establish_connection() failed, failed to cancel recv timeout\n");

    LOG_DEBUG("Sender: exit sender_establish_connection()\n");
    return 0;
}

int send_packet(uint32_t packet_num)
{
    LOG_DEBUG("Sender: into send_packet()\n");

    // packet_num ∈ [1, max_packet_num]
    if (packet_num < 1 || packet_num > max_packet_num)
        LOG_FATAL("Sender: send_packet() failed, unexpected packet_num %d\n", packet_num);
    else if (packet_num == max_packet_num && file_size % PAYLOAD_MAX != 0)
    {
        fseek(fp, (packet_num - 1) * PAYLOAD_MAX, SEEK_SET);
        char tmp[PAYLOAD_MAX];
        fread(tmp, sizeof(char), file_size % PAYLOAD_MAX, fp);
        rtp_sendto(send_fd, tmp, packet_num, file_size % PAYLOAD_MAX, 0, (struct sockaddr *)&receiver_addr, receiver_addr_len);
    }
    else if (packet_num != max_packet_num || file_size % PAYLOAD_MAX == 0)
    {
        fseek(fp, (packet_num - 1) * PAYLOAD_MAX, SEEK_SET);
        char tmp[PAYLOAD_MAX];
        fread(tmp, sizeof(char), PAYLOAD_MAX, fp);
        rtp_sendto(send_fd, tmp, packet_num, PAYLOAD_MAX, 0, (struct sockaddr *)&receiver_addr, receiver_addr_len);
    }
    else
    {
        LOG_FATAL("Sender: send_packet() failed, unexpected situation\n");
    }

    LOG_DEBUG("Sender: exit send_packet()\n");
    return 0;
}

int send_data_GBN(char *window_size_str)
{
    LOG_DEBUG("\n");
    LOG_DEBUG("Sender: into send_data_GBN()\n");

    struct timeval window_moving_timer_start, window_moving_timer_end;
    struct timeval socket_block_start, socket_block_end;
    uint32_t window_size = atoi(window_size_str);

    // 如果没有数据，直接退出
    if (max_packet_num == 0)
    {
        LOG_DEBUG("Sender: exit send_data_GBN(), no data to send\n");
        return 0;
    }

    // 设置接收超时，如果 0.1s 收不到报文就说明 receiver 没有可以发送的 ACK 了，重传
    // 其实希望使用 clock 就可以处理超时情况，但是可能出现 receiver 不传输报文情况，就需要设置套接字超时来避免一直堵塞
    // 但是只使用套接字超时也不能处理超时，因为可能套接字一直不超时但是实际上 window 已经很长时间不移动了
    // 所以要同时使用 clock 和 套接字超时 ，共同判断是否超时
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 设置超时时间为 0.1 秒
    if (setsockopt(send_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        LOG_SYS_FATAL("setsockopt() in send_data_GBN() failed, failed to set recv timeout\n");

    // 发送第一个 window ，主要是为了初始化计时器，但是如果 window 过大（大于32），不要发送那么多，可能会拥塞导致 window 最后那个数据包一直重传
    LOG_DEBUG("\n");
    LOG_DEBUG("Sender: send the first data window\n");
    for (uint32_t i = 0; i < window_size && i < max_packet_num && i < 32; i++)
    {
        send_packet(next_seq_num);
        next_seq_num++;
    }
    gettimeofday(&window_moving_timer_start, NULL);

// [send_base, next_seq_num - 1]: 已发送但未收到 ACK 的数据包
// [next_seq_num, send_base + window_size - 1]: 未发送且未收到 ACK 的数据包
#ifdef LDEBUG
    int loop_count = 1;
#endif
    while (1)
    {
        LOG_DEBUG("\n");
        LOG_DEBUG("Sender: send data loop_count: %d\n", loop_count++);
        LOG_DEBUG("Sender: send_base: %d, next_seq_num: %d\n", send_base, next_seq_num);

        errno = 0;
        gettimeofday(&window_moving_timer_end, NULL);
        gettimeofday(&socket_block_start, NULL);
        rtp_packet_t *packet = rtp_recvfrom(send_fd, (struct sockaddr *)&receiver_addr, &receiver_addr_len, false);
        gettimeofday(&socket_block_end, NULL);

        // // 收到的 seq_num 无论怎样都应该不大于 next_seq_num，但是可能 receiver 出错，所以实际情况不一定总不大于
        // if (packet != NULL)
        // {
        //     assert(packet->rtp.seq_num <= next_seq_num);
        // }

        // 如果 ACK 超时（套接字超时），重传
        if (packet == NULL && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            LOG_DEBUG("Sender: resend all not acknowledged data packet, socket timer timeout\n");
            get_time_interval_by_second(&socket_block_start, &socket_block_end);
            for (uint32_t i = send_base; i < next_seq_num; i++)
                send_packet(i);
            gettimeofday(&window_moving_timer_start, NULL);
        }
        // 如果 ACK 超时（普通超时），重传
        else if (get_time_interval_by_second(&window_moving_timer_start, &window_moving_timer_end) > PACKET_TIMEOUT_SEC)
        {
            LOG_DEBUG("Sender: resend all not acknowledged data packet, window moving timer timeout\n");
            get_time_interval_by_second(&window_moving_timer_start, &window_moving_timer_end);
            for (uint32_t i = send_base; i < next_seq_num; i++)
                send_packet(i);
            gettimeofday(&window_moving_timer_start, NULL);
            free(packet);
        }
        // 收到不合法的报文，丢弃
        else if (packet == NULL)
        {
            LOG_DEBUG("Sender: receive invalid packet\n");
        }
        // 收到的是不用回复的报文，丢弃
        else if (packet->rtp.seq_num <= send_base || packet->rtp.seq_num > next_seq_num || packet->rtp.flags != RTP_ACK)
        {
            LOG_DEBUG("Sender: receive packet that doesn't need to reply\n");
            free(packet);
        }
        // 如果可以移动 window ，重置计时器
        else if (send_base < packet->rtp.seq_num && packet->rtp.seq_num <= next_seq_num && packet->rtp.flags == RTP_ACK)
        {
            LOG_DEBUG("Sender: move window, [%d, %d] -> [%d, %d]\n", send_base, send_base + window_size - 1, packet->rtp.seq_num, packet->rtp.seq_num + window_size - 1);
            send_base = packet->rtp.seq_num;
            gettimeofday(&window_moving_timer_start, NULL);
            free(packet);
        }
        // 如果出现没有处理的情况
        else
        {
            LOG_DEBUG("next_seq_num: %d\n", next_seq_num);
            LOG_DEBUG("packet seq num: %d, packet flags: %d\n", packet->rtp.seq_num, packet->rtp.flags);
            LOG_FATAL("Sender: send_data_GBN() failed, unexpected situation when trying to send data\n");
        }

        // 无论发生什么，如果能的话，都发一个 window 内的新数据包
        if (next_seq_num <= send_base + window_size - 1 && next_seq_num <= max_packet_num)
        {
            LOG_DEBUG("Sender: send a new data packet\n");
            send_packet(next_seq_num);
            next_seq_num++;
        }
        // 如果不能发，判断是不是已经ack了所有数据包，退出
        else if (send_base > max_packet_num)
        {
            LOG_DEBUG("Sender: all data packet has been acknowledged\n");
            break;
        }

        LOG_DEBUG("Sender: send_base: %d, next_seq_num: %d\n", send_base, next_seq_num);
    }

    LOG_DEBUG("Sender: exit send_data_GBN()\n");
    return 0;
}

int send_data_SR(char *window_size_str)
{
    LOG_DEBUG("\n");
    LOG_DEBUG("Sender: into send_data_SR()\n");

    struct timeval window_moving_timer_start, window_moving_timer_end;
    uint32_t window_size = atoi(window_size_str);
    LOG_DEBUG("Sender: window_size: %d\n", window_size);
    std::set<uint32_t> not_acknowledged_packet;
    for (uint32_t i = 1; i <= window_size && i <= max_packet_num; i++)
        not_acknowledged_packet.insert(i);

    // 如果没有数据，直接退出
    if (max_packet_num == 0)
    {
        LOG_DEBUG("Sender: exit send_data_SR(), no data to send\n");
        return 0;
    }

    // 设置接收超时，如果 0.1s 收不到报文就说明 receiver 没有可以发送的 ACK 了，重传
    // 其实希望使用 clock 就可以处理超时情况，但是可能出现 receiver 不传输报文情况，就需要设置套接字超时来避免一直堵塞
    // 但是只使用套接字超时也不能处理超时，因为可能套接字一直不超时但是实际上 window 已经很长时间不移动了
    // 所以要同时使用 clock 和 套接字超时 ，共同判断是否超时
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 设置超时时间为 0.1 秒
    if (setsockopt(send_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        LOG_SYS_FATAL("setsockopt() in send_data_SR() failed, failed to set recv timeout\n");

    // 发送第一个 window ，主要是为了初始化计时器，但是如果 window 过大（大于32），不要发送那么多，可能会拥塞导致 window 最后那个数据包一直重传
    LOG_DEBUG("\n");
    LOG_DEBUG("Sender: send the first data window\n");
    for (uint32_t i = 0; i < window_size && i < max_packet_num && i < 32; i++)
    {
        send_packet(next_seq_num);
        next_seq_num++;
    }
    gettimeofday(&window_moving_timer_start, NULL);

// [send_base, next_seq_num - 1]: 已发送但未收到 ACK 的数据包
// [next_seq_num, send_base + window_size - 1]: 未发送且未收到 ACK 的数据包
#ifdef LDEBUG
    int loop_count = 1;
#endif
    while (1)
    {
        LOG_DEBUG("\n");
        LOG_DEBUG("Sender: loop_count: %d\n", loop_count++);
        LOG_DEBUG("Sender: send_base: %d, next_seq_num: %d\n", send_base, next_seq_num);
        if (not_acknowledged_packet.empty())
            LOG_DEBUG("Sender: not_acknowledged_packet is empty\n");
        else
        {
            LOG_DEBUG("Sender: not_acknowledged_packet is not empty\n");
            LOG_DEBUG("Sender: not_acknowledged_packet: \n");
        }

        errno = 0;
        gettimeofday(&window_moving_timer_end, NULL);
        rtp_packet_t *packet = rtp_recvfrom(send_fd, (struct sockaddr *)&receiver_addr, &receiver_addr_len, false);
        // 收到的 seq_num 无论怎样都应该不大于 next_seq_num，但是可能 receiver 出错，所以实际情况不一定总不大于
        if (packet != NULL)
        {
            LOG_DEBUG("received packet seq num: %d, received packet length: %d, received packet flags: %d\n", packet->rtp.seq_num, packet->rtp.length, packet->rtp.flags);

            // assert(packet->rtp.seq_num <= next_seq_num);

            if (not_acknowledged_packet.empty())
                LOG_DEBUG("Sender: not_acknowledged_packet is empty\n");
            else
            {
                LOG_DEBUG("Sender: not_acknowledged_packet is not empty\n");
                LOG_DEBUG("Sender: not_acknowledged_packet: \n");
                LOG_DEBUG("Sender: ");
#ifdef LDEBUG
                for (auto i : not_acknowledged_packet)
                    printf("%d ", i);
                printf("\n");
#endif
            }
            assert(not_acknowledged_packet.size() <= window_size);
        }

        // 如果 ACK 超时（套接字超时），重传
        if (packet == NULL && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            LOG_DEBUG("Sender: resend all not acknowledged data packet, socket timer timeout\n");
            get_time_interval_by_second(&window_moving_timer_start, &window_moving_timer_end);
            for (auto i : not_acknowledged_packet)
                send_packet(i);
            gettimeofday(&window_moving_timer_start, NULL);
        }
        // 如果 ACK 超时（普通超时），重传
        else if (get_time_interval_by_second(&window_moving_timer_start, &window_moving_timer_end) > PACKET_TIMEOUT_SEC)
        {
            LOG_DEBUG("Sender: resend all not acknowledged data packet, window moving timer timeout\n");
            get_time_interval_by_second(&window_moving_timer_start, &window_moving_timer_end);
            for (auto i : not_acknowledged_packet)
                send_packet(i);
            gettimeofday(&window_moving_timer_start, NULL);
            free(packet);
        }
        // 收到不合法的报文，丢弃
        else if (packet == NULL)
        {
            LOG_DEBUG("Sender: receive invalid packet\n");
        }
        // 收到的是不用处理的报文，丢弃
        else if (packet->rtp.seq_num < send_base || packet->rtp.seq_num >= next_seq_num || packet->rtp.flags != RTP_ACK)
        {
            LOG_DEBUG("Sender: receive packet that doesn't need to reply\n");
            free(packet);
        }
        // 如果收到可用数据包，标记 ACK ，检查是否可以移动窗口
        else if (send_base <= packet->rtp.seq_num && packet->rtp.seq_num < next_seq_num && packet->rtp.flags == RTP_ACK)
        {
            uint32_t acked_packet_num = packet->rtp.seq_num;
            // 新的 ACK
            if (not_acknowledged_packet.find(acked_packet_num) != not_acknowledged_packet.end())
            {
                LOG_DEBUG("Sender: receive new ACK packet, acked packet num: %d\n", acked_packet_num);
                not_acknowledged_packet.erase(acked_packet_num);
                // 可以移动窗口，移动窗口并重置计时器
                if (not_acknowledged_packet.empty() || send_base < *not_acknowledged_packet.begin())
                {
                    uint32_t next_send_base = (not_acknowledged_packet.empty() ? next_seq_num : *not_acknowledged_packet.begin());
                    LOG_DEBUG("Sender: move window, [%d, %d] -> [%d, %d]\n", send_base, send_base + window_size - 1, next_send_base, next_send_base + window_size - 1);
                    send_base = next_send_base;
                    gettimeofday(&window_moving_timer_start, NULL);
                }
                // 不能移动窗口
                else
                {
                    LOG_DEBUG("Sender: can't move window, [%d, %d]\n", send_base, send_base + window_size - 1);
                }
            }
            // 重复的 ACK，不做处理
            else
            {
                LOG_DEBUG("Sender: receive ACKed ACK packet, but the packet has been acknowledged, acked packet num: %d\n", acked_packet_num);
            }
            free(packet);
        }
        // 如果出现没有处理的情况
        else
        {
            LOG_DEBUG("next_seq_num: %d\n", next_seq_num);
            LOG_DEBUG("packet seq num: %d, packet flags: %d\n", packet->rtp.seq_num, packet->rtp.flags);
            LOG_FATAL("Sender: send_data_SR() failed, unexpected situation when trying to send data\n");
        }

        LOG_DEBUG("\n");
        LOG_DEBUG("After processing:\n");
        LOG_DEBUG("Sender: send_base: %d, next_seq_num: %d\n", send_base, next_seq_num);
        if (not_acknowledged_packet.empty())
            LOG_DEBUG("Sender: not_acknowledged_packet is empty\n");
        else
        {
            LOG_DEBUG("Sender: not_acknowledged_packet is not empty\n");
            LOG_DEBUG("Sender: not_acknowledged_packet: \n");
            LOG_DEBUG("Sender: ");
#ifdef LDEBUG
            for (auto i : not_acknowledged_packet)
                printf("%d ", i);
            printf("\n");
#endif
        }

        // 无论发生什么，如果能的话，都发一个 window 内的新数据包
        if (next_seq_num <= send_base + window_size - 1 && next_seq_num <= max_packet_num)
        {
            send_packet(next_seq_num);
            not_acknowledged_packet.insert(next_seq_num);
            next_seq_num++;
        }
        // 如果不能发，判断是不是已经ack了所有数据包，退出
        else if (send_base > max_packet_num)
            break;
    }

    LOG_DEBUG("Sender: exit send_data_SR()\n");
    return 0;
}

int sender_terminate_connection()
{
    LOG_DEBUG("\n");
    LOG_DEBUG("Sender: into sender_terminate_connection()\n");

    struct timeval start, end;

    // 发送第一次挥手报文
    LOG_DEBUG("Sender: send the first termination packet\n");
    rtp_sendto(send_fd, NULL, next_seq_num, 0, RTP_FIN, (struct sockaddr *)&receiver_addr, receiver_addr_len);
    gettimeofday(&start, NULL);
    next_seq_num++;

    // 设置接收超时，如果 0.1s 收不到报文就说明 receiver 没能收到第一次挥手报文，重传
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 设置超时时间为 0.1 秒
    if (setsockopt(send_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        LOG_SYS_FATAL("setsockopt() in sender_terminate_connection() failed, failed to set recv timeout\n");

    // 接收第二次挥手报文
    LOG_DEBUG("\n");
    LOG_DEBUG("Sender: receive the second termination packet\n");
    while (1)
    {
        errno = 0;
        gettimeofday(&end, NULL);
        rtp_packet_t *packet = rtp_recvfrom(send_fd, (struct sockaddr *)&receiver_addr, &receiver_addr_len, false);
        // 如果第二次挥手报文的 ACK 超时（套接字超时），重传
        if (packet == NULL && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            LOG_DEBUG("Sender: resend the first termination packet\n");
            rtp_sendto(send_fd, NULL, next_seq_num - 1, 0, RTP_FIN, (struct sockaddr *)&receiver_addr, receiver_addr_len);
            gettimeofday(&start, NULL);
            // break; // 测试用
        }
        // 如果第二次挥手报文的 ACK 超时（普通超时），重传
        else if (get_time_interval_by_second(&start, &end) > PACKET_TIMEOUT_SEC)
        {
            LOG_DEBUG("Sender: resend the first termination packet\n");
            rtp_sendto(send_fd, NULL, next_seq_num - 1, 0, RTP_FIN, (struct sockaddr *)&receiver_addr, receiver_addr_len);
            gettimeofday(&start, NULL);
            free(packet);
        }
        // 如果收到的是不合法的报文，丢弃
        else if (packet == NULL)
            continue;
        // 收到不用回复的报文，丢弃
        else if (packet->rtp.seq_num != next_seq_num - 1 || packet->rtp.flags != (RTP_FIN | RTP_ACK))
        {
            free(packet);
            continue;
        }
        // 收到正确的第二次挥手报文，可以退出
        else if (packet->rtp.seq_num == next_seq_num - 1 && packet->rtp.flags == (RTP_FIN | RTP_ACK))
        {
            free(packet);
            break;
        }
        else
        {
            LOG_DEBUG("next_seq_num: %d\n", next_seq_num);
            LOG_DEBUG("packet seq num: %d, packet flags: %d\n", packet->rtp.seq_num, packet->rtp.flags);
            LOG_FATAL("Sender: sender_terminate_connection() failed, unexpected situation when trying to receive the second termination packet\n");
        }
    }

    LOG_DEBUG("Sender: exit sender_terminate_connection()\n");
    return 0;
}

int delete_sender()
{
    LOG_DEBUG("Sender: into delete_sender()\n");

    close(send_fd);
    fclose(fp);

    LOG_DEBUG("Sender: exit delete_sender()\n");
    return 0;
}