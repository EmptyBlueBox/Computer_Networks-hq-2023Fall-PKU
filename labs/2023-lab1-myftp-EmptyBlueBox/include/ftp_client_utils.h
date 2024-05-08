#pragma once
#include <string>

#define MAGIC_NUMBER_LENGTH 6
#define BUF_SIZE 4096
#define HEADER_SIZE 12

enum FTP_CLIENT_STATUS
{
    IDLE,
    CONNECTED,
};

enum FTP_CLIENT_COMMAND
{
    OPEN,
    LS,
    GET,
    PUT,
    SHA256,
    QUIT,
    INVALID
};

const std::string FTP_CLIENT_COMMAND_STR[] = {
    "open",
    "ls",
    "get",
    "put",
    "sha256",
    "quit",
    "invalid"};

struct DATA_STREAM
{
    u_int8_t m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes)，使用有符号数，防止报错 */
    u_int8_t m_type;                          /* type (1 byte) */
    u_int8_t m_status;                        /* status (1 byte) */
    u_int32_t m_length;                       /* length (4 bytes) in Big endian*/
    char payload[];                           /* payload (length bytes) */
} __attribute__((packed));

extern FTP_CLIENT_STATUS status; // 在不同文件中使用同一个变量需要使用 extern 关键字
extern std::string connecting;

/**
 * @brief 分析命令，返回 IP 和 Port 组成的字符串
 *
 * @param command
 * @return std::string
 */
std::string get_ip_port(char *command);

/**
 * @brief 解析命令
 *
 * @param command
 * @return FTP_CLIENT_COMMAND
 */
enum FTP_CLIENT_COMMAND parse_command(char *command);

/**
 * @brief 检查协议是否正确
 *
 * @param data_stream
 * @return true
 */
bool check_protocol(DATA_STREAM *data_stream);