#ifndef __RTP_H
#define __RTP_H

#include <stdint.h>
// #include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/socket.h>

#define PAYLOAD_MAX 1461
#define PACKET_TIMEOUT_SEC 0.1
#define OFFSET 0x417C1016
    // #define OFFSET 0

    // flags in the rtp header
    typedef enum RtpHeaderFlag
    {
        RTP_SYN = 0b0001,
        RTP_ACK = 0b0010,
        RTP_FIN = 0b0100,
    } rtp_header_flag_t;

    typedef struct __attribute__((__packed__)) RtpHeader
    {
        uint32_t seq_num;  // Sequence number
        uint16_t length;   // Length of data; 0 for SYN, ACK, and FIN packets
        uint32_t checksum; // 32-bit CRC
        uint8_t flags;     // See at `RtpHeaderFlag`
    } rtp_header_t;

    typedef struct __attribute__((__packed__)) RtpPacket
    {
        rtp_header_t rtp;          // header
        char payload[PAYLOAD_MAX]; // data
    } rtp_packet_t;

    /**
     * @brief Send a rtp packet to the destination, wrapper of UDP, with checksum
     * @param[in] sockfd The socket file descriptor
     * @param[in] msg The message to be sent
     * @param[in] seq_num The sequence number of the packet
     * @param[in] length The length of the message, without the length of the header
     * @param[in] flags The flags of the packet, see at `RtpHeaderFlag`
     * @param[in] to The destination address
     * @param[in] tolen The length of the destination address
     * @return The number of bytes sent on success, -1 on failure
     * @author Yutong LIANG
     * @date 11/4/2023
     */
    int rtp_sendto(int sockfd, const void *msg, int seq_num, int length, int flags, struct sockaddr *to, socklen_t tolen);

    /**
     * @brief Receive a rtp packet from the source, wrapper of UDP, with checksum
     * @param[in] sockfd The socket file descriptor
     * @param[out] from The source address
     * @param[out] fromlen The length of the source address
     * @param[in] exit_when_time_out Whether to exit when timeout, default is true
     * @return The received packet on success, NULL on failure
     * @note The caller should free the memory of the received packet after use
     * @author Yutong LIANG
     * @date 11/4/2023
     */
    rtp_packet_t *rtp_recvfrom(int sockfd, struct sockaddr *from, socklen_t *fromlen, bool exit_when_time_out = true);

#ifdef __cplusplus
}
#endif

#endif // __RTP_H
