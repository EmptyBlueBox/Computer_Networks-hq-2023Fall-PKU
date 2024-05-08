#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <utility>

#define MAX_PORT_NUM 256
#define MAX_LEAF_NODE_NUM 512

#define HEADER_LENGTH 12
#define MAX_PAYLOAD_LENGTH (16384 - 12)

#define TYPE_DV 0
#define TYPE_DATA 1
#define TYPE_CONTROL 2

#define TRIGGER_DV_SEND '0'
#define RELEASE_NAT_ITEM '1'
#define PORT_VALUE_CHANGE '2'
#define ADD_HOST '3'
#define BLOCK_ADDR '5'
#define UNBLOCK_ADDR '6'

typedef std::pair<uint32_t, int> leaf_node_t;

// Use it to display debug information. Turn it on/off in CMakeLists.txt
#define DEBUG
#ifdef DEBUG
#define LOG_DEBUG(...)                                         \
    do                                                         \
    {                                                          \
        fprintf(stderr, "\033[33m[DEBUG]\033[0m" __VA_ARGS__); \
        fflush(stderr);                                        \
    } while (0)
#else
#define LOG_DEBUG(...)
#endif

// Use it when an unrecoverable error happened
#define LOG_FATAL(...)                                         \
    do                                                         \
    {                                                          \
        fprintf(stderr, "\033[31m[FATAL]\033[0m" __VA_ARGS__); \
        fflush(stderr);                                        \
        exit(EXIT_FAILURE);                                    \
    } while (0)

void parse_ip_addr(char *ip_addr, leaf_node_t *leaf)
{
    LOG_DEBUG("into parse_ip_addr()\n");
    char *slash = strchr(ip_addr, '/');
    if (slash == NULL)
    {
        LOG_FATAL("Invalid IP address: %s\n", ip_addr);
    }
    *slash = '\0';

    // get IP address
    leaf->first = (uint32_t)inet_addr(ip_addr);
    if (leaf->first == INADDR_NONE)
    {
        LOG_FATAL("Invalid IP address: %s\n", ip_addr);
    }
    leaf->first = ntohl(leaf->first);

    // get subnet mask
    leaf->second = atoi(slash + 1);
    if (leaf->second < 0 || leaf->second > 32)
    {
        LOG_FATAL("Invalid subnet mask: %d\n", leaf->second);
    }
    leaf->second = 32 - leaf->second;
    leaf->first = ((leaf->first) >> (leaf->second)) << (leaf->second); // 将IP地址的后subnet_mask位置0

    *slash = '/';
    LOG_DEBUG("    Parsed IP address: %s, addr: %08x, subnet mask: %d\n", ip_addr, leaf->first, leaf->second);
    LOG_DEBUG("exit parse_ip_addr()\n");
    return;
}

#endif