#ifndef COMPNET_LAB4_SRC_SWITCH_H
#define COMPNET_LAB4_SRC_SWITCH_H

#include "types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

class SwitchBase
{
public:
  SwitchBase() = default;
  ~SwitchBase() = default;

  virtual void InitSwitch(int numPorts) = 0;
  virtual int ProcessFrame(int inPort, char *framePtr) = 0;
};

extern SwitchBase *CreateSwitchObject();
extern int PackFrame(char *unpacked_frame, char *packed_frame, int frame_length);
extern int UnpackFrame(char *unpacked_frame, char *packed_frame, int frame_length);

/**
 * @brief MAC address entry class
 * @note This class is used to store MAC address entry in MAC table of switch
 * @author LIANG Yutong
 * @date 1/9/2024
 */
class mac_entry_t
{
public:
  mac_addr_t dst;
  int port;
  int counter;
  bool is_valid;
};

/**
 * @brief Switch class
 * @note This class is used to simulate a switch
 * @author LIANG Yutong
 * @date 1/9/2024
 */
class EthernetSwitch : public SwitchBase
{
private:
  int port_num;
  mac_entry_t mac_table[MAX_PORT_NUM];

public:
  void InitSwitch(int numPorts) override;
  int ProcessFrame(int inPort, char *framePtr) override;
};

// #define DEBUG
#ifdef DEBUG
#define LOG_DEBUG(...)                                     \
  do                                                       \
  {                                                        \
    fprintf(stderr, "\033[33m[DEBUG]\033[0m" __VA_ARGS__); \
    fflush(stderr);                                        \
  } while (0)
#else
#define LOG_DEBUG(...)
#endif

#endif // ! COMPNET_LAB4_SRC_SWITCH_H
