/**
 * @file rtp.cpp
 * @brief Implementation of the functions in rtp.h
 * @note The functions are used in receiver_functions.c and sender_functions.c
 * @note The basic ideas of the functions are wrapping the rtp packet inside the rtp_sendto() and rtp_recvfrom() functions
 * @note We don't want to explicitly construct the rtp_packet_t struct in receiver_functions.c and sender_functions.c
 * @author Yutong LIANG
 * @date 11/4/2023
 */

#include "rtp.h"
#include "util.h"
#include <string.h>

int rtp_sendto(int sockfd, const void *msg, int seq_num, int length, int flags, struct sockaddr *to, socklen_t tolen)
{
    LOG_DEBUG("into rtp_sendto()\n");

    rtp_packet_t send_packet;
    send_packet.rtp.seq_num = seq_num + OFFSET; // 加上偏移量
    send_packet.rtp.length = length;
    send_packet.rtp.checksum = 0;
    send_packet.rtp.flags = flags;

    if (length)
        memcpy(send_packet.payload, msg, length);
    send_packet.rtp.checksum = compute_checksum((void *)&send_packet, sizeof(rtp_header_t) + length);

    int sent = sendto(sockfd, &send_packet, sizeof(rtp_header_t) + length, 0, to, tolen);
    if (sent == -1)
        LOG_SYS_FATAL("    sendto() in rtp_sendto() failed, sent = %d\n, sendto() time out\n", sent);
    else if (sent == 0)
        LOG_SYS_FATAL("    sendto() in rtp_sendto() failed, sent = %d\n, connection closed by peer\n", sent);
    else if (sent != (int)sizeof(rtp_header_t) + length) // 当我们通过UDP发出的包总是符合MTU的限制时，我们的包一定可以一次性发出，而不会被分片
        LOG_SYS_FATAL("    sendto() in rtp_sendto() failed, sent = %d\n, sendto() not complete\n", sent);

    LOG_DEBUG("    rtp_sendto() seq_num = %X, payload length = %d, flags = %d, checksum = %d\n", send_packet.rtp.seq_num, send_packet.rtp.length, send_packet.rtp.flags, send_packet.rtp.checksum);
    LOG_DEBUG("    rtp_sendto() already sent %d bytes, in hex:\n", sent);
    print_hex((void *)&send_packet, sent);
    LOG_DEBUG("exit rtp_sendto()\n");

    return 0;
}

rtp_packet_t *rtp_recvfrom(int sockfd, struct sockaddr *from, socklen_t *fromlen, bool exit_when_time_out)
{
    LOG_DEBUG("into rtp_recvfrom()\n");

    void *tmp_packet = malloc(sizeof(rtp_header_t) + PAYLOAD_MAX);
    int received = recvfrom(sockfd, tmp_packet, sizeof(rtp_header_t) + PAYLOAD_MAX, 0, from, fromlen);
    rtp_packet_t *received_packet = (rtp_packet_t *)tmp_packet;

    uint32_t checksum;
    if (received == -1)
    {
        free(received_packet);
        if (exit_when_time_out)
            LOG_SYS_FATAL("    recvfrom() in rtp_recvfrom() failed, received = %d, recvfrom() time out\n", received);
        else
        {
            LOG_DEBUG("    recvfrom() in rtp_recvfrom() failed, received = %d, recvfrom() time out\n", received);
            LOG_DEBUG("exit rtp_recvfrom()\n");
            return NULL;
        }
    }
    else if (received == 0)
    {
        free(received_packet);
        LOG_SYS_FATAL("    recvfrom() in rtp_recvfrom() failed, received = %d, connection closed by peer\n", received);
    }
    else
    {
        checksum = received_packet->rtp.checksum;
        received_packet->rtp.checksum = 0;
        if (checksum != compute_checksum((void *)received_packet, received)) // 如果checksum不对，那么就丢弃这个包
        {
            free(received_packet);
            LOG_DEBUG("    rtp_recvfrom() failed, wrong checksum = %d\n, real checksum = %d\n", checksum, compute_checksum((void *)received_packet, received));
            LOG_DEBUG("exit rtp_recvfrom()\n");
            return NULL;
        }
        else if (received_packet->rtp.flags & 0b11111000) // 如果flags不对，那么就丢弃这个包
        {
            free(received_packet);
            LOG_DEBUG("    rtp_recvfrom() failed, wrong flags = %d\n", received_packet->rtp.flags);
            LOG_DEBUG("exit rtp_recvfrom()\n");
            return NULL;
        }
    }

    LOG_DEBUG("    rtp_recvfrom() seq_num = %X, payload length = %d, flags = %d, checksum = %d\n", received_packet->rtp.seq_num, received_packet->rtp.length, received_packet->rtp.flags, checksum);
    LOG_DEBUG("    rtp_recvfrom() already received %d bytes, in hex:\n", received);
    received_packet->rtp.checksum = checksum;
    print_hex((void *)received_packet, received);
    received_packet->rtp.seq_num -= OFFSET; // 减去偏移量
    LOG_DEBUG("exit rtp_recvfrom()\n");

    return received_packet;
}