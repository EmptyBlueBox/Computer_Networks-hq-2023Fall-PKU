#include <fcntl.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

pid_t StartController(int &read_fd, int &write_fd)
{
  int read_fds[2], write_fds[2];
  if (pipe(read_fds) != 0)
  {
    std::cerr << "GTest: failed to create pipe to simulator" << std::endl;
    return -1;
  }
  if (pipe(write_fds) != 0)
  {
    std::cerr << "GTest: failed to create pipe to simulator" << std::endl;
    return -1;
  }
  pid_t status = fork();
  if (status == -1)
  {
    std::cerr << "GTest: failed to create sub-process" << std::endl;
    return -1;
  }
  if (status == 0)
  {
    /* child process */
    int empty_fd = open("/dev/null", O_RDWR);
    read_fd = write_fds[0];
    write_fd = read_fds[1];
    close(write_fds[1]);
    close(read_fds[0]);
    dup2(read_fd, STDIN_FILENO);
    // dup2(empty_fd, STDOUT_FILENO);
    // dup2(empty_fd, STDERR_FILENO);
    close(empty_fd);
    const char **args = new const char *[4];
    args[0] = "simulator";
    args[1] = "1";                              // test mode
    args[2] = std::to_string(write_fd).c_str(); // pass write fd
    args[3] = nullptr;

    if (execv("./simulator", (char *const *)args) == -1)
    {
      std::cerr << "GTest: Failed to start simulator" << std::endl;
      exit(-1);
    }
  }
  else
  {
    read_fd = read_fds[0];
    write_fd = write_fds[1];
    close(read_fds[1]);
    close(write_fds[0]);
    fcntl(read_fd, F_SETFL, O_NONBLOCK);
    fcntl(write_fd, F_SETFL, O_NONBLOCK);
    char buf[32];
    int bytes = 0;
    for (int i = 0; i < 10; i++)
    {
      usleep(100000);
      bytes = read(read_fd, buf, 1);
      if (bytes > 0)
        break;
    }
    if (bytes <= 0)
      std::cerr << "GTest: simulator may not start working properly"
                << std::endl;
  }
  return status;
}

void ClearProcess(pid_t pid)
{
  kill(pid, SIGKILL);
  usleep(500000);
}

int ClearProcessAndWaitExit(pid_t pid)
{
  ClearProcess(pid);
  int status;
  auto retval = waitpid(pid, &status, WNOHANG);
  if (retval != pid)
    return -1;
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  if (WIFSTOPPED(status))
    std::cerr << "Signal" << std::endl;
  return -1;
}

int WaitExit(pid_t pid)
{
  usleep(500000);
  int status;
  auto retval = waitpid(pid, &status, WNOHANG);
  if (retval != pid)
    return -1;
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  if (WIFSTOPPED(status))
    std::cerr << "Signal" << std::endl;
  return -1;
}

int Prepare(int &read_fd, int &write_fd, int &controller_pid)
{
  controller_pid = StartController(read_fd, write_fd);
  EXPECT_GT(controller_pid, 0);
  if (controller_pid <= 0)
    return -1;
  return 0;
}

static int send(int fd, char *buf, int len)
{
  int times = 0, ret;
  while (times++ < 100 && (ret = write(fd, buf, len)) == -1)
    usleep(15000);
  return ret;
}

static int recv(int fd, char *buf)
{
  int times = 0, ret;
  while (times++ < 100 && (ret = read(fd, buf, 9999)) == -1)
    usleep(15000);
  return ret;
}

static void SendNew(int fd, int numPort)
{
  char buf[256];
  int len = sprintf(buf, "new %d\n", numPort);
  if (send(fd, buf, len) == -1)
    std::cerr << "GTest: failed to send to simulator" << std::endl;
}

static int RecvNew(int fd, int &v)
{
  char buf[9999];
  int ret = recv(fd, buf);
  if (ret != -1)
  {
    sscanf(buf, "%d", &v);
  }
  return ret;
}

static void SendAddHost(int fd, int switchId, char *addr)
{
  char buf[256];
  int len = sprintf(buf, "addhost %d %s\n", switchId, addr);
  if (send(fd, buf, len) == -1)
  {
    std::cerr << "GTest: failed to send to simulator" << std::endl;
  }
}

static void SendLink(int fd, int switch1, int switch2)
{
  char buf[256];
  int len = sprintf(buf, "link %d %d\n", switch1, switch2);
  if (send(fd, buf, len) == -1)
  {
    std::cerr << "GTest: failed to send to simulator" << std::endl;
  }
}

static void send_hostbroadcast(int fd, char *src, char *dst, char *payload)
{
  char cmdBuf[9999];
  int len = sprintf(cmdBuf, "hostbroadcast %s %s %s\n", src, dst, payload);
  if (send(fd, cmdBuf, len) == -1)
  {
    std::cerr << "GTest : send_broadcast failed to send to simulator"
              << std::endl;
  }
}

static void SendNS(int fd)
{
  if (send(fd, "ns\n", 3) == -1)
    std::cerr << "GTest: next step failed to send to simulator" << std::endl;
}

static void SendHostsend(int fd, char *src, char *dst, char *payload)
{
  char buf[9999];
  int len = sprintf(buf, "hostsend %s %s %s\n", src, dst, payload);
  if (send(fd, buf, len) == -1)
    std::cerr << "GTest: HostSend failed to send to simulator" << std::endl;
}

static int RecvHostsend(int fd, int &len, char *src, char *payload)
{
  char buf[9999];
  int ret = recv(fd, buf);
  if (ret != -1)
    sscanf(buf, "%d %s %s\n", &len, src, payload);
  return ret;
}

static void SendAging(int fd)
{
  char msgBuf[256];
  int len = sprintf(msgBuf, "aging\n");
  msgBuf[len] = 0;
  if (send(fd, msgBuf, len))
  {
    std::cerr << "GTest: Aging failed to send to simulator" << std::endl;
  }
}

static int RecvAging(int fd)
{
  char buf[128];
  int ret = recv(fd, buf);
  if (ret != -1)
    sscanf(buf, "%d\n", &ret);
  return ret;
}

static void SendWarmup(int fd)
{
  char msgBuf[256];
  int len = sprintf(msgBuf, "warmup\n");
  msgBuf[len] = 0;
  if (send(fd, msgBuf, len))
  {
    std::cerr << "GTest: Aging failed to send to simulator" << std::endl;
  }
}

static int RecvWarmup(int fd)
{
  char buf[128];
  int ret = recv(fd, buf);
  if (ret != -1)
    sscanf(buf, "%d\n", &ret);
  return ret;
}

static void SendExit(int fd)
{
  if (send(fd, "exit\n", 5) == -1)
  {
    std::cerr << "GTest: failed to send to simulator" << std::endl;
  }
}

class Topology1 : public testing::Test
{
public:
  void SetUp() { ASSERT_EQ(Prepare(readFd, writeFd, controllerPid), 0); }
  void TearDown() { ClearProcessAndWaitExit(controllerPid); }

  int readFd;
  int writeFd;
  int controllerPid;
};

class Topology2 : public testing::Test
{
public:
  void SetUp() { ASSERT_EQ(Prepare(readFd, writeFd, controllerPid), 0); }
  void TearDown() { ClearProcessAndWaitExit(controllerPid); }

  int readFd;
  int writeFd;
  int controllerPid;
};

class Topology3 : public testing::Test
{
public:
  void SetUp() { ASSERT_EQ(Prepare(readFd, writeFd, controllerPid), 0); }
  void TearDown() { ClearProcessAndWaitExit(controllerPid); }

  int readFd;
  int writeFd;
  int controllerPid;
};

class Topology4 : public testing::Test
{
public:
  void SetUp() { ASSERT_EQ(Prepare(readFd, writeFd, controllerPid), 0); }
  void TearDown() { ClearProcessAndWaitExit(controllerPid); }

  int readFd;
  int writeFd;
  int controllerPid;
};

class Topology5 : public testing::Test
{
public:
  void SetUp() { ASSERT_EQ(Prepare(readFd, writeFd, controllerPid), 0); }
  void TearDown() { ClearProcessAndWaitExit(controllerPid); }

  int readFd;
  int writeFd;
  int controllerPid;
};

class Topology6 : public testing::Test
{
public:
  void SetUp() { ASSERT_EQ(Prepare(readFd, writeFd, controllerPid), 0); }
  void TearDown() { ClearProcessAndWaitExit(controllerPid); }

  int readFd;
  int writeFd;
  int controllerPid;
};

TEST_F(Topology1, Forwarding)
{
  // New Switch
  SendNew(writeFd, 4);

  int switchId, ret;
  ret = RecvNew(readFd, switchId);
  if (ret == -1)
  {
    std::cerr << "Create new Switch fail" << std::endl;
  }

  const int kNumHost = 3;
  char hostEtherAddrs[kNumHost][19] = {"de:ad:be:ef:00:01", "de:ad:be:ef:00:02",
                                       "de:ad:be:ef:00:03"};
  for (int i = 0; i < kNumHost; i++)
  {
    SendAddHost(writeFd, switchId, hostEtherAddrs[i]);
  }

  // Warmup the MAC Table of each switch.
  //  for (int i = 0; i < kNumHost; i++) {
  //    for (int j = 0; j < kNumHost; j++) {
  //      char msgBuf[256] = {};
  //      if (i == j) {
  //        continue;
  //      }
  //      sprintf(msgBuf, "HelloFrom%dTo%d", i, j);
  //      send_hostbroadcast(writeFd, hostEtherAddrs[i], hostEtherAddrs[j],
  //      msgBuf); SendNS(writeFd);
  //    }
  //  }

  std::cerr << "GTest : Start warmup send" << std::endl;
  SendWarmup(writeFd);
  std::cerr << "GTest : Warmup send done" << std::endl;
  ret = RecvWarmup(readFd);
  std::cerr << "GTest : Warmup recv done, ret = " << ret << std::endl;
  ASSERT_NE(ret, -1);
  if (ret == -1)
  {
    std::cerr << "GTest : Warmup forwarding table fails" << std::endl;
  }

  int len = 0;
  char srcAddr[20];
  char originBuf[256], resultBuf[256];

  std::cerr << "GTest : Start forwarding test" << std::endl;
  for (int i = 0; i < kNumHost; i++)
  {
    for (int j = 0; j < kNumHost; j++)
    {
      if (i != j)
      {
        bzero(originBuf, sizeof(originBuf));
        bzero(resultBuf, sizeof(resultBuf));

        for (int k = 0; k < 254; k++)
        {
          originBuf[k] = rand() % 26 + 'a';
        }

        SendHostsend(writeFd, hostEtherAddrs[i], hostEtherAddrs[j], originBuf);
        RecvHostsend(readFd, len, srcAddr, resultBuf);

        EXPECT_EQ(len, 1);
        EXPECT_EQ(strcmp(originBuf, resultBuf), 0);
      }
    }
  }

  SendExit(writeFd);
  int retVal = WaitExit(controllerPid);
  EXPECT_GE(retVal, 0);
  return;
}

TEST_F(Topology1, SwitchAging)
{
  // New Switch
  SendNew(writeFd, 4);

  int switchId, ret;
  ret = RecvNew(readFd, switchId);
  ASSERT_NE(ret, -1);

  if (ret == -1)
  {
    std::cerr << "Create new Switch fail" << std::endl;
  }

  const int kNumHost = 3;
  char hostEtherAddrs[kNumHost][19] = {"de:ad:be:ef:00:01", "de:ad:be:ef:00:02",
                                       "de:ad:be:ef:00:03"};
  for (int i = 0; i < kNumHost; i++)
  {
    SendAddHost(writeFd, switchId, hostEtherAddrs[i]);
  }

  int len = 0;
  char srcAddr[20];
  char msgBuf[256], resultBuf[256];

  // Warmup the MAC Table of each switch.
  //  for (int i = 0; i < kNumHost; i++) {
  //    for (int j = 0; j < kNumHost; j++) {
  //      if (i == j) {
  //        continue;
  //      }
  //
  //      bzero(msgBuf, sizeof(msgBuf));
  //      sprintf(msgBuf, "HelloFrom%dTo%d", i, j);
  //      send_hostbroadcast(writeFd, hostEtherAddrs[i], hostEtherAddrs[j],
  //      msgBuf); SendNS(writeFd);
  //    }
  //  }
  SendWarmup(writeFd);
  EXPECT_NE(RecvWarmup(readFd), -1);

  bzero(msgBuf, sizeof(msgBuf));
  sprintf(msgBuf, "HelloFromTopology1AgingTest");

  // Make sure the MAC Table is correct.
  for (int i = 0; i < kNumHost; i++)
  {
    for (int j = 0; j < kNumHost; j++)
    {
      if (i != j)
      {
        bzero(resultBuf, sizeof(resultBuf));
        SendHostsend(writeFd, hostEtherAddrs[i], hostEtherAddrs[j], msgBuf);
        RecvHostsend(readFd, len, srcAddr, resultBuf);
        EXPECT_EQ(len, 1);
        EXPECT_EQ(strcmp(msgBuf, resultBuf), 0);
      }
    }
  }

  // Send Aging message to controller.
  for (int i = 0; i < 15; i++)
  {
    SendAging(writeFd);
    RecvAging(readFd);
  }

  // Start aging test : now 0 -> 1 && 1 -> 2 should fail with broadcast error.
  for (int i = 0; i < kNumHost - 1; i++)
  {
    int j = (i + 1) % kNumHost;
    SendHostsend(writeFd, hostEtherAddrs[i], hostEtherAddrs[j], msgBuf);
    RecvHostsend(readFd, len, srcAddr, resultBuf);
    EXPECT_EQ(len, -3);
  }

  SendExit(writeFd);
  int retVal = WaitExit(controllerPid);
  EXPECT_GE(retVal, 0);
  return;
}

TEST_F(Topology2, Forwarding)
{
  const int kNumSwitch = 3;
  const int kNumHost = 3;
  int switchId[kNumSwitch] = {};
  int numSwitchPort[kNumSwitch] = {3, 4, 3};
  char hostAddress[kNumHost][20] = {"de:ad:be:ef:00:01", "de:ad:be:ef:00:02",
                                    "de:ad:be:ef:00:03"};

  // Create switch.
  for (int i = 0; i < kNumSwitch; i++)
  {
    SendNew(writeFd, numSwitchPort[i]);
    RecvNew(readFd, switchId[i]);
  }

  // Create Host: link host[i] with switch[i].
  for (int i = 0; i < kNumHost; i++)
  {
    SendAddHost(writeFd, switchId[i], hostAddress[i]);
  }

  // Link switch: link switch[i] with switch[i+1].
  for (int i = 0; i < kNumSwitch - 1; i++)
  {
    SendLink(writeFd, switchId[i], switchId[i + 1]);
  }

  // Warmup the MAC Table of each switch.
  //  for (int i = 0; i < kNumHost; i++) {
  //    for (int j = 0; j < kNumHost; j++) {
  //      if (i != j) {
  //        char msgBuf[256];
  //        sprintf(msgBuf, "HelloFromHost%dtoHost%d.", i, j);
  //        send_hostbroadcast(writeFd, hostAddress[i], hostAddress[j], msgBuf);
  //        SendNS(writeFd);
  //      }
  //    }
  //  }
  SendWarmup(writeFd);
  ASSERT_NE(-1, RecvWarmup(readFd));

  // Start forwarding test.
  int len;
  char srcAddress[32];
  char originBuf[256], resultBuf[256];
  for (int i = 0; i < kNumHost; i++)
  {
    for (int j = 0; j < kNumHost; j++)
    {
      if (i == j)
      {
        continue;
      }

      // Generate random payload.
      for (int k = 0; k < 254; k++)
      {
        originBuf[k] = rand() % 26 + 'a';
      }

      SendHostsend(writeFd, hostAddress[i], hostAddress[j], originBuf);
      RecvHostsend(readFd, len, srcAddress, resultBuf);

      int expectLen = std::abs(i - j) + 1;
      EXPECT_EQ(len, expectLen);
      EXPECT_EQ(strcmp(originBuf, resultBuf), 0);
    }
  }

  SendExit(writeFd);
  int retVal = WaitExit(controllerPid);
  EXPECT_GE(retVal, 0);
  return;
}

TEST_F(Topology2, SwitchAging)
{
  const int kNumSwitch = 3;
  const int kNumHost = 3;
  int switchId[kNumSwitch] = {};
  int numSwitchPort[kNumSwitch] = {3, 4, 3};
  char hostAddress[kNumHost][20] = {"de:ad:be:ef:00:01", "de:ad:be:ef:00:02",
                                    "de:ad:be:ef:00:03"};

  // Create switch.
  for (int i = 0; i < kNumSwitch; i++)
  {
    SendNew(writeFd, numSwitchPort[i]);
    RecvNew(readFd, switchId[i]);
  }

  // Create Host: link host[i] with switch[i].
  for (int i = 0; i < kNumHost; i++)
  {
    SendAddHost(writeFd, switchId[i], hostAddress[i]);
  }

  // Link switch: link switch[i] with switch[i+1].
  for (int i = 0; i < kNumSwitch - 1; i++)
  {
    SendLink(writeFd, switchId[i], switchId[i + 1]);
  }

  // Warmup the MAC Table of each switch.
  //  for (int i = 0; i < kNumHost; i++) {
  //    for (int j = 0; j < kNumHost; j++) {
  //      if (i != j) {
  //        char msgBuf[256] = {};
  //        sprintf(msgBuf, "HelloFromHost%dtoHost%d.", i, j);
  //        send_hostbroadcast(writeFd, hostAddress[i], hostAddress[j], msgBuf);
  //        SendNS(writeFd);
  //      }
  //    }
  //  }
  SendWarmup(writeFd);
  EXPECT_NE(RecvWarmup(readFd), -1);

  int len;
  char srcAddress[32];
  char resultBuf[256];
  char msgBuf[256];

  bzero(msgBuf, sizeof(msgBuf));
  sprintf(msgBuf, "HelloFromTopo2AgingTest");

  // Make sure the MAC Table is correct.
  for (int i = 0; i < kNumHost; i++)
  {
    for (int j = 0; j < kNumHost; j++)
    {
      if (i == j)
      {
        continue;
      }

      bzero(resultBuf, sizeof(resultBuf));
      SendHostsend(writeFd, hostAddress[i], hostAddress[j], msgBuf);
      RecvHostsend(readFd, len, srcAddress, resultBuf);

      int expectLen = std::abs(i - j) + 1;
      EXPECT_EQ(len, expectLen);
      EXPECT_EQ(strcmp(msgBuf, resultBuf), 0);
    }
  }

  // Send Aging to controller.
  for (int i = 0; i < 15; i++)
  {
    SendAging(writeFd);
    RecvAging(readFd);
  }

  // Start aging testing.
  for (int i = 0; i < kNumHost; i++)
  {
    for (int j = 0; j < kNumHost; j++)
    {
      if (i != j)
      {
        SendHostsend(writeFd, hostAddress[i], hostAddress[j], msgBuf);
        RecvHostsend(readFd, len, srcAddress, resultBuf);

        // Should result in a boradcast error.
        EXPECT_EQ(len, -3);
      }
    }
  }

  SendExit(writeFd);
  int retVal = WaitExit(controllerPid);
  EXPECT_GE(retVal, 0);
  return;
}

TEST_F(Topology3, Forwarding)
{
  const int kNumSwitch = 3;
  const int kNumHost = 6;

  char hostAddrs[kNumHost][20] = {
      "de:ad:be:ef:00:01",
      "de:ad:be:ef:00:02",
      "de:ad:be:ef:00:03",
      "de:ad:be:ef:00:04",
      "de:ad:be:ef:00:05",
      "de:ad:be:ef:00:06",
  };

  int switchIds[kNumSwitch] = {0, 0, 0};
  int numPorts[kNumSwitch] = {4, 5, 4};
  int hostToSwitch[kNumHost] = {0, 0, 1, 1, 2, 2};

  // Create switch
  for (int i = 0; i < kNumSwitch; i++)
  {
    SendNew(writeFd, numPorts[i]);
    RecvNew(readFd, switchIds[i]);
  }

  // Add Host
  for (int i = 0; i < kNumHost; i++)
  {
    SendAddHost(writeFd, switchIds[hostToSwitch[i]], hostAddrs[i]);
  }

  // Link Switch switchIds[i] with switchIds[i+1].
  for (int i = 0; i < kNumSwitch - 1; i++)
  {
    SendLink(writeFd, switchIds[i], switchIds[i + 1]);
  }

  // Warmup MAC Table.
  //  for (int i = 0; i < kNumHost; i++) {
  //    for (int j = 0; j < kNumHost; j++) {
  //      if (i != j) {
  //        char msgBuf[256] = {};
  //        sprintf(msgBuf, "From%dTo%d", i, j);
  //        send_hostbroadcast(writeFd, hostAddrs[i], hostAddrs[j], msgBuf);
  //        SendNS(writeFd);
  //      }
  //    }
  //  }

  SendWarmup(writeFd);
  ASSERT_NE(-1, RecvWarmup(readFd));

  int kDistance[kNumHost][kNumHost] = {{0, 1, 2, 2, 3, 3}, {1, 0, 2, 2, 3, 3}, {2, 2, 0, 1, 2, 2}, {2, 2, 1, 0, 2, 2}, {3, 3, 2, 2, 0, 1}, {3, 3, 2, 2, 1, 0}};

  // Start Forwarding test.
  int len;
  char srcAddress[32];
  char originBuf[256], resultBuf[256];
  for (int i = 0; i < kNumHost; i++)
  {
    for (int j = 0; j < kNumHost; j++)
    {
      if (i == j)
      {
        continue;
      }

      // Generate random payload.
      for (int k = 0; k < 254; k++)
      {
        originBuf[k] = rand() % 26 + 'a';
      }

      SendHostsend(writeFd, hostAddrs[i], hostAddrs[j], originBuf);
      RecvHostsend(readFd, len, srcAddress, resultBuf);

      EXPECT_EQ(len, kDistance[i][j]);
      EXPECT_EQ(strcmp(originBuf, resultBuf), 0);
    }
  }

  SendExit(writeFd);
  int retVal = WaitExit(controllerPid);
  EXPECT_GE(retVal, 0);
  return;
}

TEST_F(Topology3, SwitchAging)
{
  const int kNumSwitch = 3;
  const int kNumHost = 6;

  char hostAddrs[kNumHost][20] = {
      "de:ad:be:ef:00:01",
      "de:ad:be:ef:00:02",
      "de:ad:be:ef:00:03",
      "de:ad:be:ef:00:04",
      "de:ad:be:ef:00:05",
      "de:ad:be:ef:00:06",
  };

  int switchIds[kNumSwitch] = {0, 0, 0};
  int numPorts[kNumSwitch] = {4, 5, 4};
  int hostToSwitch[kNumHost] = {0, 0, 1, 1, 2, 2};

  // Create switch
  for (int i = 0; i < kNumSwitch; i++)
  {
    SendNew(writeFd, numPorts[i]);
    RecvNew(readFd, switchIds[i]);
  }

  // Add Host
  for (int i = 0; i < kNumHost; i++)
  {
    SendAddHost(writeFd, switchIds[hostToSwitch[i]], hostAddrs[i]);
  }

  // Link Switch switchIds[i] with switchIds[i+1].
  for (int i = 0; i < kNumSwitch - 1; i++)
  {
    SendLink(writeFd, switchIds[i], switchIds[i + 1]);
  }

  // Warmup MAC Table.
  //  for (int i = 0; i < kNumHost; i++) {
  //    for (int j = 0; j < kNumHost; j++) {
  //      if (i != j) {
  //        char msgBuf[256] = {};
  //        sprintf(msgBuf, "From%dTo%d", i, j);
  //        send_hostbroadcast(writeFd, hostAddrs[i], hostAddrs[j], msgBuf);
  //        SendNS(writeFd);
  //      }
  //    }
  //  }
  SendWarmup(writeFd);
  EXPECT_NE(RecvWarmup(readFd), -1);

  // Start Forwarding test.
  int len;
  char srcAddress[32];
  char originBuf[256], resultBuf[256];
  char msgBuf[256];

  int kDistance[kNumHost][kNumHost] = {{0, 1, 2, 2, 3, 3}, {1, 0, 2, 2, 3, 3}, {2, 2, 0, 1, 2, 2}, {2, 2, 1, 0, 2, 2}, {3, 3, 2, 2, 0, 1}, {3, 3, 2, 2, 1, 0}};
  int hostDest[kNumHost] = {2, 3, 4, 5, 0, 1};

  bzero(msgBuf, sizeof(msgBuf));
  sprintf(msgBuf, "HelloFromtopology3AgingTest");

  // Make sure that the MAC Table is set up.
  for (int i = 0; i < kNumHost; i++)
  {
    for (int j = 0; j < kNumHost; j++)
    {
      if (i == j)
      {
        continue;
      }

      bzero(resultBuf, sizeof(resultBuf));
      SendHostsend(writeFd, hostAddrs[i], hostAddrs[j], msgBuf);
      RecvHostsend(readFd, len, srcAddress, resultBuf);

      EXPECT_EQ(len, kDistance[i][j]);
      EXPECT_EQ(strcmp(msgBuf, resultBuf), 0);
    }
  }

  // Send Aging th controller.
  for (int i = 0; i < 15; i++)
  {
    SendAging(writeFd);
    RecvAging(readFd);
  }

  // Now the MAC Table should be cleared.
  for (int i = 0; i < kNumHost; i++)
  {
    int j = hostDest[i];
    SendHostsend(writeFd, hostAddrs[i], hostAddrs[j], msgBuf);
    RecvHostsend(readFd, len, srcAddress, resultBuf);

    // Should result in a broadcast error.
    EXPECT_EQ(len, -3);
  }

  SendExit(writeFd);
  int retVal = WaitExit(controllerPid);
  EXPECT_GE(retVal, 0);
  return;
}

TEST_F(Topology4, Forwarding)
{
  const int kNumHost = 4;
  const int kNumSwitch = 2;
  int switchIds[kNumSwitch] = {0, 0};
  char hostEtherAddrs[kNumHost][19] = {"de:ad:be:ef:00:01", "de:ad:be:ef:00:02",
                                       "de:ad:be:ef:00:03",
                                       "de:ad:be:ef:00:04"};
  for (int i = 0; i < kNumSwitch; i++)
  {
    SendNew(writeFd, 4);
    int ret = RecvNew(readFd, switchIds[i]);
    if (ret == -1)
    {
      std::cerr << "Create new Switch fail" << std::endl;
    }
  }

  for (int i = 0; i < kNumSwitch - 1; i++)
  {
    SendLink(writeFd, switchIds[i], switchIds[i + 1]);
  }

  for (int i = 0; i < kNumHost; i++)
  {
    int switchIndex = i / 2;
    SendAddHost(writeFd, switchIds[switchIndex], hostEtherAddrs[i]);
  }

  // Warmup the MAC Table of each switch.
  //  for (int i = 0; i < kNumHost; i++) {
  //    for (int j = 0; j < kNumHost; j++) {
  //      char msgBuf[256] = {};
  //      if (i == j) {
  //        continue;
  //      }
  //      sprintf(msgBuf, "HelloFrom%dTo%d", i, j);
  //      send_hostbroadcast(writeFd, hostEtherAddrs[i], hostEtherAddrs[j],
  //      msgBuf); SendNS(writeFd);
  //    }
  //  }
  SendWarmup(writeFd);
  EXPECT_NE(RecvWarmup(readFd), -1);

  int len = 0;
  char srcAddr[20];
  char originBuf[256], resultBuf[256];

  for (int i = 0; i < kNumHost; i++)
  {
    for (int j = 0; j < kNumHost; j++)
    {
      if (i != j)
      {
        bzero(originBuf, sizeof(originBuf));
        bzero(resultBuf, sizeof(resultBuf));

        for (int k = 0; k < 254; k++)
        {
          originBuf[k] = rand() % 26 + 'a';
        }

        SendHostsend(writeFd, hostEtherAddrs[i], hostEtherAddrs[j], originBuf);
        RecvHostsend(readFd, len, srcAddr, resultBuf);

        int si = i / 2;
        int sj = j / 2;
        int expectLen = std::abs(si - sj) + 1;

        EXPECT_EQ(len, expectLen);
        EXPECT_EQ(strcmp(originBuf, resultBuf), 0);
      }
    }
  }

  SendExit(writeFd);
  int retVal = WaitExit(controllerPid);
  EXPECT_GE(retVal, 0);
  return;
}

TEST_F(Topology4, Mixing)
{
  const int kNumHost = 4;
  const int kNumSwitch = 2;
  int switchIds[kNumSwitch] = {0, 0};
  char hostEtherAddrs[kNumHost][19] = {"de:ad:be:ef:00:01", "de:ad:be:ef:00:02",
                                       "de:ad:be:ef:00:03",
                                       "de:ad:be:ef:00:04"};
  for (int i = 0; i < kNumSwitch; i++)
  {
    SendNew(writeFd, 4);
    int ret = RecvNew(readFd, switchIds[i]);
    if (ret == -1)
    {
      std::cerr << "Create new Switch fail" << std::endl;
    }
  }

  for (int i = 0; i < kNumSwitch - 1; i++)
  {
    SendLink(writeFd, switchIds[i], switchIds[i + 1]);
  }

  for (int i = 0; i < kNumHost; i++)
  {
    int switchIndex = i / 2;
    SendAddHost(writeFd, switchIds[switchIndex], hostEtherAddrs[i]);
  }

  int len = 0;
  char srcAddr[20];
  char msgBuf[256], resultBuf[256];

  SendWarmup(writeFd);
  EXPECT_NE(RecvWarmup(readFd), -1);

  bzero(msgBuf, sizeof(msgBuf));
  sprintf(msgBuf, "HelloFromTopology4MixingTest");

  // Make sure the MAC Table is correct.
  for (int i = 0; i < kNumHost; i++)
  {
    for (int j = 0; j < kNumHost; j++)
    {
      if (i != j)
      {
        bzero(resultBuf, sizeof(resultBuf));
        SendHostsend(writeFd, hostEtherAddrs[i], hostEtherAddrs[j], msgBuf);
        RecvHostsend(readFd, len, srcAddr, resultBuf);

        int si = i / 2;
        int sj = j / 2;
        int expectLen = std::abs(si - sj) + 1;

        EXPECT_EQ(len, expectLen);
        EXPECT_EQ(strcmp(msgBuf, resultBuf), 0);
      }
    }
  }

  // Send Aging message to controller.
  for (int i = 0; i < 7; i++)
  {
    SendAging(writeFd);
    RecvAging(readFd);
  }

  // generatng random pair
  int src = rand() % kNumHost;
  int dest = rand() % kNumHost;
  while (dest == src)
  {
    dest = rand() % kNumHost;
  }

  // This dest -> src should not be flushed.
  SendHostsend(writeFd, hostEtherAddrs[dest], hostEtherAddrs[src], msgBuf);
  RecvHostsend(readFd, len, srcAddr, resultBuf);
  EXPECT_NE(len, -3);

  for (int i = 0; i < 7; i++)
  {
    SendAging(writeFd);
    RecvAging(readFd);
  }

  // This src -> dest should not be flushed.
  SendHostsend(writeFd, hostEtherAddrs[src], hostEtherAddrs[dest], msgBuf);
  RecvHostsend(readFd, len, srcAddr, resultBuf);
  EXPECT_NE(len, -3);

  SendExit(writeFd);
  int retVal = WaitExit(controllerPid);
  EXPECT_GE(retVal, 0);
  return;
}

int _tmain(int argc, char *argv[])
{
  srand(time(NULL));
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
