#include "rtp.h"
#include "util.h"
#include "receiver_functions.h"

/**
 * @brief The main function of the receiver
 *
 * @param[in] argc The number of arguments
 * @param[in] argv The arguments: [./receiver] [listen port] [file path] [window size] [mode]
 * @return The exit status
 *
 * @note assert all arguments are valid
 * @note The mode is either 0 = "gbn" or 1 = "sr"
 *
 * @author Yutong LIANG
 * @date 11/3/2023
 */
int main(int argc, char **argv)
{
    if (argc != 5)
        LOG_FATAL("Wrong number of arguments\nUsage: ./receiver [listen port] [file path] [window size] [mode]\n");

    init_receiver(argv[1], argv[2]);
    receiver_establish_connection();

    if (argv[4][0] == '0')
        receive_data_GBN(argv[3]);
    else if (argv[4][0] == '1')
        receive_data_SR(argv[3]);
    else
        LOG_FATAL("Wrong mode\n");

    receiver_terminate_connection();
    delete_receiver();

    LOG_DEBUG("Receiver: exit\n");
    return 0;
}
