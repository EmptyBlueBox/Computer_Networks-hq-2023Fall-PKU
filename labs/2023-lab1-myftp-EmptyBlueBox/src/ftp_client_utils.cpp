#include "ftp_client_utils.h"
#include <sstream>
#include <vector>

FTP_CLIENT_STATUS status = FTP_CLIENT_STATUS::IDLE;
std::string connecting = "none";

std::string get_ip_port(char *command)
{
    std::string command_str(command);
    std::stringstream ss(command_str);
    std::string token;
    std::vector<std::string> args;
    while (ss >> token)
        args.push_back(token);

    return args[1] + " : " + args[2];
}

enum FTP_CLIENT_COMMAND parse_command(char *command)
{
    std::string command_str(command);
    std::stringstream ss(command_str);
    std::string token;
    ss >> token;
    for (int i = 0; i < 6; i++)
    {
        if (token == FTP_CLIENT_COMMAND_STR[i])
            return static_cast<FTP_CLIENT_COMMAND>(i);
    }
    return FTP_CLIENT_COMMAND::INVALID;
}

bool check_protocol(DATA_STREAM *data_stream)
{
    u_int8_t protocol[MAGIC_NUMBER_LENGTH] = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')};
    for (int i = 0; i < MAGIC_NUMBER_LENGTH; i++)
        if (data_stream->m_protocol[i] != protocol[i])
            return false;
    return true;
}