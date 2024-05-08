#include "switch.h"

int CountOneBits(char value)
{
  int count = 0;
  for (int i = 0; i < 8; ++i)
    if (value & (1 << i))
      count++;
  return count;
}

void print_hex(const void *ptr, std::size_t len)
{
#ifdef DEBUG
  const unsigned char *c = reinterpret_cast<const unsigned char *>(ptr);
  for (std::size_t i = 0; i < len; ++i)
  {
    if (i % 8 == 0)
      LOG_DEBUG("    ");
    printf("%02X ", c[i]);
    if (i % 8 == 7)
      printf("\n"); // 每8个字节换行
  }
  printf("\n"); // 打印完成后换行
#endif
}

bool mac_addr_equal(const mac_addr_t &a, const mac_addr_t &b)
{
  LOG_DEBUG("into mac_addr_equal\n");
  LOG_DEBUG("    addr a:\n");
  print_hex(a, ETH_ALEN);
  LOG_DEBUG("    addr b:\n");
  print_hex(b, ETH_ALEN);
  for (int i = 0; i < ETH_ALEN; ++i)
    if (a[i] != b[i])
    {
      LOG_DEBUG("exit mac_addr_equal: false\n");
      return false;
    }
  LOG_DEBUG("exit mac_addr_equal: true\n");
  return true;
}

SwitchBase *CreateSwitchObject()
{
  return new EthernetSwitch;
}

int PackFrame(char *unpacked_frame, char *packed_frame, int frame_length)
{
  LOG_DEBUG("into PackFrame\n");
  fflush(stderr);
  LOG_DEBUG("    unpacked_frame:\n");
  print_hex(unpacked_frame, frame_length);
  LOG_DEBUG("    frame length = %d, payload = %s\n", ((ether_header_t *)unpacked_frame)->length, unpacked_frame + sizeof(ether_header_t));

  // 检测长度是否合法
  if (((ether_header_t *)unpacked_frame)->length != frame_length - sizeof(ether_header_t))
  {
    LOG_DEBUG("exit PackFrame: 帧长度不匹配\n");
    return -1;
  }

  // 合法的帧，进行封装
  int p1 = 0, p2 = 0; // p1: unpacked_frame pointer; p2: packed_frame pointer
  packed_frame[p2++] = (char)FRAME_DELI;
  for (p1 = 0; p1 < frame_length; ++p1)
  {
    if (unpacked_frame[p1] == (char)FRAME_DELI)
    {
      packed_frame[p2++] = (char)FRAME_DELI;
      packed_frame[p2++] = (char)FRAME_DELI;
    }
    else
      packed_frame[p2++] = unpacked_frame[p1];
  }

  // 加入奇偶校验字节
  int count_1 = 0;
  for (int i = 0; i < p2; ++i)
    count_1 += CountOneBits(packed_frame[i]);
  LOG_DEBUG("    count_1 = %d\n", count_1);
  if (count_1 % 2 == 0)
    packed_frame[p2++] = 0;
  else
    packed_frame[p2++] = 1;

  LOG_DEBUG("    packed_frame:\n");
  print_hex(packed_frame, p2);
  LOG_DEBUG("exit PackFrame\n");

  return p2;
}

int UnpackFrame(char *unpacked_frame, char *packed_frame, int frame_length)
{
  LOG_DEBUG("into UnpackFrame\n");
  fflush(stderr);
  LOG_DEBUG("    packed_frame:\n");
  print_hex(packed_frame, frame_length);

  // 没有首部的帧界定符：校验出错
  if (packed_frame[0] != (char)FRAME_DELI)
  {
    LOG_DEBUG("    packed_frame[0] = %02X\n, FRAME_DELI = %02X\n", packed_frame[0], FRAME_DELI);
    LOG_DEBUG("exit UnpackFrame: 没有首部的帧界定符\n");
    return -1;
  }
  // 有奇数个1：校验出错
  int count_1 = 0;
  for (int i = 0; i < frame_length; ++i)
    count_1 += CountOneBits(packed_frame[i]);
  LOG_DEBUG("    count_1 = %d\n", count_1);
  if (count_1 % 2)
  {
    LOG_DEBUG("exit UnpackFrame: 有奇数个 1\n");
    return -1;
  }

  int p1 = 0, p2 = 1;                       // p1: unpacked_frame pointer; p2: packed_frame pointer
  for (p2 = 1; p2 < frame_length - 1; ++p2) // 去掉首部的帧界定符和尾部的校验位
  {
    if (packed_frame[p2] == (char)FRAME_DELI)
    {
      if (packed_frame[p2 + 1] == (char)FRAME_DELI)
      {
        unpacked_frame[p1++] = (char)FRAME_DELI;
        p2++;
      }
      else // 帧界定符后面不是帧界定符，说明出错，在这个规则下除了第一个字节，其他帧界定符都是成对出现的
      {
        LOG_DEBUG("exit UnpackFrame: 帧界定符后面不是帧界定符\n");
        return -1;
      }
    }
    else
      unpacked_frame[p1++] = packed_frame[p2];
  }

  LOG_DEBUG("    unpacked_frame:\n");
  print_hex(unpacked_frame, p1);
  LOG_DEBUG("    frame length = %d, payload = %s\n", ((ether_header_t *)unpacked_frame)->length, unpacked_frame + sizeof(ether_header_t));

  if (((ether_header_t *)unpacked_frame)->length != p1 - sizeof(ether_header_t))
  {
    LOG_DEBUG("exit UnpackFrame: 帧长度不匹配\n");
    return -1;
  }

  LOG_DEBUG("exit UnpackFrame\n");

  return p1;
}

void EthernetSwitch::InitSwitch(int numPorts)
{
  port_num = numPorts;
  for (int i = 0; i < MAX_PORT_NUM; ++i)
  {
    mac_table[i].counter = 0;
    mac_table[i].is_valid = 0;
  }
}

int EthernetSwitch::ProcessFrame(int inPort, char *framePtr)
{
  LOG_DEBUG("into ProcessFrame\n");
  fflush(stderr);
  LOG_DEBUG("    inPort = %d\n", inPort);
  ether_header_t *eth_header = (ether_header_t *)framePtr;

  if (eth_header->ether_type == ETHER_DATA_TYPE && inPort != 1)
  {
    // Data frame
    LOG_DEBUG("    Data frame:\n");
    print_hex(framePtr, eth_header->length + sizeof(ether_header_t));
    LOG_DEBUG("    length = %d, payload = %s\n", eth_header->length, framePtr + sizeof(ether_header_t));

    mac_addr_t src, dst;
    memcpy(src, eth_header->ether_src, ETH_ALEN);
    memcpy(dst, eth_header->ether_dest, ETH_ALEN);
    LOG_DEBUG("    src:\n");
    print_hex(src, ETH_ALEN);
    LOG_DEBUG("    dst:\n");
    print_hex(dst, ETH_ALEN);

    bool found = false;

    // 寻找是否有匹配src的mac表项，若有则更新计数器，若无则添加
    for (int i = 0; i < MAX_PORT_NUM; ++i)
    {
      if (mac_table[i].is_valid && mac_addr_equal(mac_table[i].dst, src))
      {
        mac_table[i].counter = ETHER_MAC_AGING_THRESHOLD;
        found = true;
        break;
      }
    }
    if (!found)
    {
      for (int i = 0; i < MAX_PORT_NUM; ++i)
      {
        if (!mac_table[i].is_valid)
        {
          memcpy(mac_table[i].dst, src, ETH_ALEN);
          mac_table[i].port = inPort;
          mac_table[i].counter = ETHER_MAC_AGING_THRESHOLD;
          mac_table[i].is_valid = true;
          break;
        }
      }
    }

    // 寻找是否有匹配dst的mac表项，若有则转发，若无则广播
    found = false;
    for (int i = 0; i < MAX_PORT_NUM; ++i)
    {
      if (mac_table[i].is_valid && mac_addr_equal(mac_table[i].dst, dst))
      {
        found = true;
        if (mac_table[i].port == inPort) // 发现应该转发的端口和入端口相同，说明有环路，丢弃
        {
          LOG_DEBUG("    Loop, abandon\n");
          LOG_DEBUG("exit ProcessFrame\n");
          return -1;
        }
        else
        {
          LOG_DEBUG("    Forward to port %d\n", mac_table[i].port);
          LOG_DEBUG("exit ProcessFrame\n");
          return mac_table[i].port; // 返回应该转发的端口
        }
      }
    }
    if (!found) // 未找到匹配dst的mac表项，广播至所有端口（非入端口）
    {
      LOG_DEBUG("    Broadcast, except port %d\n", inPort);
      LOG_DEBUG("exit ProcessFrame\n");
      return 0;
    }
  }
  else if (eth_header->ether_type == ETHER_CONTROL_TYPE && inPort == 1)
  {
    LOG_DEBUG("    Control frame\n");
    // Control frame
    for (int i = 0; i < MAX_PORT_NUM; ++i)
    {
      if (mac_table[i].is_valid)
      {
        mac_table[i].counter--;
        if (mac_table[i].counter == ETHER_COMMAND_TYPE_AGING)
          mac_table[i].is_valid = 0; // 丢弃过期的mac表项
      }
    }
    LOG_DEBUG("exit ProcessFrame\n");
    return -1; // 丢弃控制帧
  }
  else
  {
    // Unknown frame type
    LOG_DEBUG("    Unknown frame type\n");
    LOG_DEBUG("exit ProcessFrame\n");
    return 0;
  }

  LOG_DEBUG("exit ProcessFrame\n");
  return 0;
}