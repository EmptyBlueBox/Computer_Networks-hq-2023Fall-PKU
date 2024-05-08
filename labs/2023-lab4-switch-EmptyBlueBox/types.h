#ifndef COMPNET_LAB4_TYPES_H
#define COMPNET_LAB4_TYPES_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>

typedef uint8_t mac_addr_t[6];
typedef std::chrono::time_point<std::chrono::steady_clock> timestamp_t;

#define ETH_ALEN 6       // length of mac address
#define MAX_PORT_NUM 512 // max port number of switch

const uint16_t ETHER_DATA_TYPE = 0;
const uint16_t ETHER_CONTROL_TYPE = 1;

// MAC Table aging.
const uint32_t ETHER_COMMAND_TYPE_AGING = 0;
const int32_t ETHER_MAC_AGING_THRESHOLD = 10;

// frame head byte
const uint8_t FRAME_DELI = 0xDE;
const uint32_t MAX_PACKED_FRAME = 3090;

#pragma pack(2)
typedef struct
{
  mac_addr_t ether_dest;
  mac_addr_t ether_src;
  uint16_t ether_type;
  uint16_t length;
} ether_header_t;
#pragma pack()

#endif // ! COMPNET_LAB4_TYPES_H