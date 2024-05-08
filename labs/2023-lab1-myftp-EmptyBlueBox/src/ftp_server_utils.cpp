#include "ftp_server_utils.h"

FTP_SERVER_STATUS status = FTP_SERVER_STATUS::IDLE;

bool check_protocol(DATA_STREAM *data_stream)
{
    u_int8_t protocol[MAGIC_NUMBER_LENGTH] = {u_int8_t(0xc1), u_int8_t(0xa1), u_int8_t(0x10), u_int8_t('f'), u_int8_t('t'), u_int8_t('p')};
    for (int i = 0; i < MAGIC_NUMBER_LENGTH; i++)
        if (data_stream->m_protocol[i] != protocol[i])
            return false;
    return true;
}