#pragma once
#include <string>

#define MAGIC_NUMBER_LENGTH 6
#define BUF_SIZE 4096
#define HEADER_SIZE 12
#define LISTENQ 64

enum FTP_SERVER_STATUS
{
    IDLE,
    CONNECTED,
};

enum COMMAND_TYPE
{
    OPEN_CONN_REQUEST = 0xA1,
    OPEN_CONN_REPLY = 0xA2,
    LIST_REQUEST = 0xA3,
    LIST_REPLY = 0xA4,
    GET_REQUEST = 0xA5,
    GET_REPLY = 0xA6,
    FILE_DATA = 0xFF,
    PUT_REQUEST = 0xA7,
    PUT_REPLY = 0xA8,
    SHA_REQUEST = 0xA9,
    SHA_REPLY = 0xAA,
    QUIT_REQUEST = 0xAB,
    QUIT_REPLY = 0xAC
};

struct DATA_STREAM
{
    u_int8_t m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes)，使用有符号数，防止报错 */
    u_int8_t m_type;                          /* type (1 byte) */
    u_int8_t m_status;                        /* status (1 byte) */
    u_int32_t m_length;                       /* length (4 bytes) in Big endian*/
    char payload[];                           /* payload (length bytes) */
} __attribute__((packed));

extern FTP_SERVER_STATUS status;
/**
 * @brief 检查协议是否正确
 *
 * @param data_stream
 * @return true
 */
bool check_protocol(DATA_STREAM *data_stream);