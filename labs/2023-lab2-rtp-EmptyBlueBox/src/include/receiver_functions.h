/**
 * @file receiver_functions.h
 * @brief Declaration of the functions in receiver_functions.c
 * @note The functions are used in receiver.c
 * @author Yutong LIANG
 * @date 11/4/2023
 */

#ifndef __RECEIVER_FUNCTIONS_H
#define __RECEIVER_FUNCTIONS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/socket.h>
#include <stdio.h>

    /**
     * @brief Initialize the receiver
     *
     * The function create a socket, bind the socket to a port, and open the file to write
     *
     * @param[in] listen_port The port on which the receiver listens
     * @param[in] file_path The path of the file to write
     * @return The exit status
     * @note This function modifies the global variables receive_fd, receiver_addr, and fp
     * @note The function set the timeout of the socket to 5s
     * @author Yutong LIANG
     * @date 11/4/2023
     */
    int init_receiver(char *listen_port, char *file_path);

    /**
     * @brief Establish the connection between the receiver and the sender
     * @return The exit status
     * @note This function modifies the global variable sender_addr
     * @author Yutong LIANG
     * @date 11/4/2023
     */
    int receiver_establish_connection();

    /**
     * @brief Receive the data from the sender using GBN
     * @param[in] window_size The window size
     * @return The exit status
     * @note This function modifies the global variable next_seq_num, sender_addr, and sender_addr_len
     * @note The function will write the data to the file
     * @author Yutong LIANG
     * @date 11/4/2023
     */
    int receive_data_GBN(char *window_size);

    /**
     * @brief Receive the data from the sender using SR
     * @param[in] window_size The window size
     * @return The exit status
     * @note This function modifies the global variable next_seq_num, sender_addr, and sender_addr_len
     * @note The function will write the data to the file
     * @author Yutong LIANG
     * @date 11/4/2023
     */
    int receive_data_SR(char *window_size);

    /**
     * @brief Terminate the connection between the receiver and the sender
     * @return The exit status
     * @note This function modifies the global variable sender_addr and sender_addr_len
     * @author Yutong LIANG
     * @date 11/5/2023
     */
    int receiver_terminate_connection();

    /**
     * @brief Delete the receiver
     *
     * The function close the socket and the file
     *
     * @return The exit status
     * @note This function modifies the global variables receive_fd and fp
     * @author Yutong LIANG
     * @date 11/4/2023
     */
    int delete_receiver();

#ifdef __cplusplus
}
#endif

#endif // __RECEIVER_FUNCTIONS_H