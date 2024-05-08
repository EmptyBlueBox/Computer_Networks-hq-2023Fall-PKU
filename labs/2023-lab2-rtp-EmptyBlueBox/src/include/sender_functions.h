#ifndef __SENDER_FUNCTIONS_H
#define __SENDER_FUNCTIONS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <cstdint>

    /**
     * @brief Initialize the sender
     *
     * The function create a socket, bind the socket to a port, and open the file to read
     *
     * @param[in] remote_IP The IP address of the receiver
     * @param[in] remote_port The port of the receiver
     * @param[in] file_path The path of the file to read
     * @return The exit status
     * @note This function modifies the global variables send_fd, sender_addr, and fp
     * @note The function set the timeout of the socket to 5s
     * @author Yutong LIANG
     * @date 11/5/2023
     */
    int init_sender(char *remote_IP, char *remote_port, char *file_path);

    /**
     * @brief Establish the connection between the sender and the receiver
     * @return The exit status
     * @note This function modifies the global variable receiver_addr
     * @author Yutong LIANG
     * @date 11/5/2023
     */
    int sender_establish_connection();

    /**
     * @brief Send the one data packet to the receiver
     * @param[in] packet_num The number of the packet to send
     * @return The exit status
     * @note This function modifies the global variable sender_addr and sender_addr_len
     * @note The function will read the data from the file
     * @author Yutong LIANG
     * @date 11/5/2023
     */
    int send_packet(uint32_t packet_num);

    /**
     * @brief Send the data to the receiver using GBN
     * @param[in] window_size_str The window size
     * @return The exit status
     * @note This function modifies the global variable sender_addr and sender_addr_len
     * @note The function will read the data from the file
     * @author Yutong LIANG
     * @date 11/5/2023
     */
    int send_data_GBN(char *window_size_str);

    /**
     * @brief Send the data to the receiver using SR
     * @param[in] window_size_str The window size
     * @return The exit status
     * @note This function modifies the global variable sender_addr and sender_addr_len
     * @note The function will read the data from the file
     * @author Yutong LIANG
     * @date 11/5/2023
     */
    int send_data_SR(char *window_size_str);

    /**
     * @brief Terminate the connection between the sender and the receiver
     * @return The exit status
     * @note This function modifies the global variable sender_addr and sender_addr_len
     * @author Yutong LIANG
     * @date 11/5/2023
     */
    int sender_terminate_connection();

    /**
     * @brief Delete the sender
     *
     * The function close the socket and the file
     *
     * @return The exit status
     * @note This function modifies the global variables send_fd and fp
     * @author Yutong LIANG
     * @date 11/5/2023
     */
    int delete_sender();

#ifdef __cplusplus
}
#endif

#endif // __SENDER_FUNCTIONS_H