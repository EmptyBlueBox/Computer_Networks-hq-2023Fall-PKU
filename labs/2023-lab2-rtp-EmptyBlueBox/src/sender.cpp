#include "rtp.h"
#include "util.h"
#include "sender_functions.h"

int main(int argc, char **argv)
{
    if (argc != 6)
        LOG_FATAL("Wrong number of arguments\nUsage: ./sender [remote IP] [remote port] [file path] [window size] [mode]\n");

    init_sender(argv[1], argv[2], argv[3]);
    sender_establish_connection();

    if (argv[5][0] == '0')
        send_data_GBN(argv[4]);
    else if (argv[5][0] == '1')
        send_data_SR(argv[4]);
    else
        LOG_FATAL("Wrong mode\n");

    sender_terminate_connection();
    delete_sender();

    LOG_DEBUG("Sender: exit\n");
    return 0;
}
