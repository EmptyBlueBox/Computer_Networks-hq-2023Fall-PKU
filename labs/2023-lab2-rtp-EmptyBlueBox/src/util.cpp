#include "util.h"

static uint32_t crc32_for_byte(uint32_t r)
{
    for (int j = 0; j < 8; ++j)
        r = (r & 1 ? 0 : (uint32_t)0xEDB88320L) ^ r >> 1;
    return r ^ (uint32_t)0xFF000000L;
}

static void crc32(const void *data, size_t n_bytes, uint32_t *crc)
{
    static uint32_t table[0x100];
    if (!*table)
        for (size_t i = 0; i < 0x100; ++i)
            table[i] = crc32_for_byte(i);
    for (size_t i = 0; i < n_bytes; ++i)
        *crc = table[(uint8_t)*crc ^ ((uint8_t *)data)[i]] ^ *crc >> 8;
}

// Computes checksum for `n_bytes` of data
//
// Hint 1: Before computing the checksum, you should set everything up
// and set the "checksum" field to 0. And when checking if a packet
// has the correct check sum, don't forget to set the "checksum" field
// back to 0 before invoking this function.
//
// Hint 2: `len + sizeof(rtp_header_t)` is the real length of a rtp
// data packet.
uint32_t compute_checksum(const void *pkt, size_t n_bytes)
{
    uint32_t crc = 0;
    crc32(pkt, n_bytes, &crc);
    return crc;
}

void print_hex(const void *ptr, std::size_t len)
{
#ifdef LDEBUG
    const unsigned char *c = reinterpret_cast<const unsigned char *>(ptr);
    for (std::size_t i = 0; i < len; ++i)
    {
        if (i % 8 == 0)
            LOG_DEBUG("    ");
        printf("%02X ", c[i]);
        if (i % 8 == 7)
        { // 每8个字节换行
            printf("\n");
        }
    }
    printf("\n"); // 打印完成后换行
#endif
}

double get_time_interval_by_second(struct timeval *test_start, struct timeval *test_end)
{
    long seconds = (test_end->tv_sec - test_start->tv_sec);
    long micros = ((seconds * 1000000) + test_end->tv_usec) - (test_start->tv_usec);
    double seconds_double = (double)micros / 1000000;
    LOG_DEBUG("    time interval: %f\n", seconds_double);
    return seconds_double;
}