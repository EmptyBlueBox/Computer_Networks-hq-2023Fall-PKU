/* run gtest */
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fstream>
#include <cstdlib>
#include <fcntl.h>
#include <map>
#include <set>
#include <unordered_map>
#include <algorithm>

using namespace std;

static void s2ipv4(const char* addr, uint32_t &ipv4) {
    uint32_t a, b, c, d;
    sscanf(addr, "%u.%u.%u.%u", &a, &b, &c, &d);
    ipv4 = a | (b << 8) | (c << 16) | (d << 24);
}

static void ipv42s(char* addr, uint32_t ipv4) {
    uint8_t a = ipv4 & 0xff;
    uint8_t b = (ipv4 >> 8) & 0xff;
    uint8_t c = (ipv4 >> 16) & 0xff;
    uint8_t d = (ipv4 >> 24) & 0xff;
    sprintf(addr, "%hu.%hu.%hu.%hu", a, b, c, d);
}

static void s2ipv4_with_mask(const char* addr, uint32_t &ipv4, uint32_t &mask) {
    s2ipv4(addr, ipv4);
    uint32_t a, b, c, d, e;
    sscanf(addr, "%u.%u.%u.%u/%u", &a, &b, &c, &d, &e);
    assert(e >= 0 && e <= 32);
    mask = htonl(~((1U << (32 - e)) - 1));
}

static bool is_sub_addr(uint32_t ip, char* addr_with_mask) {
    uint32_t available_addr, available_mask;
    s2ipv4_with_mask(addr_with_mask, available_addr, available_mask);
    return ((ip ^ available_addr) & available_mask) == 0;
}

static bool is_sub_addr(char* ip, char* addr_with_mask) {
    uint32_t test_ip;
    s2ipv4(ip, test_ip);
    return is_sub_addr(test_ip, addr_with_mask);
}

static const uint32_t ip_delta = (1 << 24);
static const uint32_t ip_delta2 = (1 << 16);

void sigpipe_handler(int signum) {
    cerr << "GTest: received SIGPIPE signal, sub-process may crashed" << endl;
    exit(-1);
}

pid_t start_controller(int &read_fd, int &write_fd) {
    int read_fds[2], write_fds[2];
    if(pipe(read_fds) != 0) {
        cerr << "GTest: failed to create pipe to simulator" << endl;
        return -1;
    }
    if(pipe(write_fds) != 0) {
        cerr << "GTest: failed to create pipe to simulator" << endl;
        return -1;
    }
    pid_t status = fork();
    if(status == -1) {
        cerr << "GTest: failed to create sub-process" << endl;
        return -1;
    }
    if(status == 0) {
        /* child process */
        int empty_fd = open("/dev/null", O_RDWR);
        read_fd = write_fds[0];
        write_fd = read_fds[1];
        close(write_fds[1]);
        close(read_fds[0]);
        dup2(read_fd, STDIN_FILENO);
        dup2(empty_fd, STDOUT_FILENO);
        dup2(empty_fd, STDERR_FILENO);
        close(empty_fd);
        const char** args = new const char*[4];
        args[0] = "simulator";
        args[1] = "1";                          // test mode
        args[2] = to_string(write_fd).c_str();  // pass write fd 
        args[3] = nullptr;
        if(execv("./simulator", (char* const*)args) == -1) {
            cerr << "GTest: failed to start simulator" << endl;
            exit(-1);
        }
    }
    else {
        read_fd = read_fds[0];
        write_fd = write_fds[1];
        close(read_fds[1]);
        close(write_fds[0]);
        fcntl(read_fd, F_SETFL, O_NONBLOCK);
        fcntl(write_fd, F_SETFL, O_NONBLOCK);
        char buf[32];
        int bytes = 0;
        for(int i = 0; i < 10; i ++) {
            usleep(100000);
            bytes = read(read_fd, buf, 1);
            if(bytes > 0) break;
        }
        if(bytes <= 0)
            cerr << "GTest: simulator may not start working properly" << endl;
    }
    return status;
}

void clear_process(pid_t pid) {
    kill(pid, SIGKILL);
    usleep(500000);
}

int clear_process_and_wait_exit(pid_t pid) {
    clear_process(pid);
    int status;
    auto retval = waitpid(pid, &status, WNOHANG);
    if(retval != pid) return -1;
    if(WIFEXITED(status)) return WEXITSTATUS(status);
    if(WIFSTOPPED(status)) cerr << "Signal" << endl;
    return -1;
}

int wait_exit(pid_t pid) {
    usleep(500000);
    int status;
    auto retval = waitpid(pid, &status, WNOHANG);
    if(retval != pid) return -1;
    if(WIFEXITED(status)) return WEXITSTATUS(status);
    if(WIFSTOPPED(status)) cerr << "Signal" << endl;
    return -1;
}

int prepare(int &read_fd, int &write_fd, int &controller_pid) {
    controller_pid = start_controller(read_fd, write_fd);
    if(controller_pid <= 0) return -1;
    signal(SIGPIPE, sigpipe_handler);
    return 0;
}

class Routing:public testing::Test {
public:
    int read_fd, write_fd, controller_pid;
    void SetUp() {
        ASSERT_EQ(prepare(read_fd, write_fd, controller_pid), 0);
    }
    void TearDown() {
        clear_process_and_wait_exit(controller_pid);
    }
};

class NAT:public testing::Test {
public:
    int read_fd, write_fd, controller_pid;
    void SetUp() {
        ASSERT_EQ(prepare(read_fd, write_fd, controller_pid), 0);
    }
    void TearDown() {
        clear_process_and_wait_exit(controller_pid);
    }
};

class FireWall: public testing::Test {
public:
    int read_fd, write_fd, controller_pid;
    void SetUp() {
        ASSERT_EQ(prepare(read_fd, write_fd, controller_pid), 0);
    }
    void TearDown() {
        clear_process_and_wait_exit(controller_pid);
    }
};

class General:public testing::Test {
public:
    int read_fd, write_fd, controller_pid;
    void SetUp() {
        ASSERT_EQ(prepare(read_fd, write_fd, controller_pid), 0);
    }
    void TearDown() {
        clear_process_and_wait_exit(controller_pid);
    }
};


static void output_error_info(int error_num) {
    switch (error_num) {
    case -1:
        cerr << endl << "GTest: test command error, please report to TA" << endl << endl;
        break;
    case -2:
        cerr << endl << "GTest: no route to destination" << endl << endl;
        break;
    case -3:
        cerr << endl << "GTest: router sent a broadcast packet while forwarding data pakcet" << endl << endl;
        break;
    case -4:
        cerr << endl << "GTest: router unexpectedly dropped packet while forwarding data packet" << endl << endl;
        break;
    case -5:
        cerr << endl << "GTest: router returned an invalid port number while forwarding data packet" << endl << endl;
        break;
    case -6:
        cerr << endl << "GTest: too many hops while forwording data packet" << endl << endl;
        break;
    case -7:
        cerr << endl << "GTest: packet sent to wrong destination" << endl << endl;
        break;
    //case -8:
        //cerr << endl << "drop blocked packets normally" << endl;
        //break; // this is a normal case
    default:
        break;
    }
}

static int send(int fd, const char* buf, int len) {
    int times = 0, ret;
    while(times ++ < 100000 && (ret = write(fd, buf, len)) == -1) usleep(100);
    if(times == 100000) cerr << "GTest: send to simulator failed, simulator may too slow or no response" << endl;
    return ret;
}

static void send_exit(int fd) {
    if(send(fd, "exit\n", 5) == -1)
        return;
}

static void send_new(int fd, int port_num, int external_port, const char* external_addr, const char* available_addr) {
    char buf[256];
    int len = sprintf(buf, "new %d %d %s %s\n", port_num, external_port, external_addr, available_addr);
    if(send(fd, buf, len) == -1)
        return;
}

static void send_ns(int fd) {
    if(send(fd, "ns\n", 3) == -1)
        return;
}

static void send_link(int fd, int router1, int router2, int weight) {
    char buf[256];
    int len = sprintf(buf, "link %d %d %d\n", router1, router2, weight);
    if(send(fd, buf, len) == -1)
        return;
}

static void send_cut(int fd, int router1, int router2) {
    char buf[256];
    int len = sprintf(buf, "cut %d %d\n", router1, router2);
    if(send(fd, buf, len) == -1)
        return;
}

static void send_weight(int fd, int router1, int router2, int weight) {
    char buf[256];
    int len = sprintf(buf, "weight %d %d %d\n", router1, router2, weight);
    if(send(fd, buf, len) == -1)
        return;
}

static void send_addhost(int fd, int router, char* addr) {
    char buf[256];
    int len = snprintf(buf, 255, "addhost %d %s\n", router, addr);
    if(send(fd, buf, len) == -1)
        return;
}

static void send_blockaddr(int fd, int router, char* addr) {
    char buf[256];
    int len = snprintf(buf, 255, "blockaddr %d %s\n", router, addr);
    if(send(fd, buf, len) == -1)
        return;
}

static void send_unblockaddr(int fd, int router, char* addr) {
    char buf[256];
    int len = snprintf(buf, 255, "unblockaddr %d %s\n", router, addr);
    if(send(fd, buf, len) == -1)
        return;
}

static void send_trigger(int fd) {
    if(send(fd, "trigger\n", 8) == -1)
        return;
}

static void send_release(int fd, int router, char* addr) {
    char buf[512];
    int len = snprintf(buf, 511, "release %d %s\n", router, addr);
    if(send(fd, buf, len) == -1)
        return;
}

static void send_hostsend(int fd, char* src, char* dst, char* payload) {
    char buf[9999];
    int len = snprintf(buf, 9998, "hostsend %s %s %s\n", src, dst, payload);
    if(send(fd, buf, len) == -1)
        return;
}

static void send_extersend(int fd, int router, const char* src, const char* dst, char* payload) {
    char buf[9999];
    int len = snprintf(buf, 9998, "extersend %d %s %s %s\n", router, src, dst, payload);
    if(send(fd, buf, len) == -1)
        return;
}

static int recv(int fd, char* buf) {
    int times = 0, ret;
    while(times ++ < 100000 && (ret = read(fd, buf, 9999)) == -1) usleep(100);
    if(times == 100000) cerr << "GTest: read from simulator failed, simulator may too slow or no response" << endl;
    return ret;
}

static int recv_new(int fd, int &v) {
    char buf[9999];
    int ret = recv(fd, buf);
    if(ret != -1) sscanf(buf, "%d", &v);
    return ret;
}

static int recv_ns(int fd, int &v) {
    char buf[9999];
    int ret = recv(fd, buf);
    if(ret != -1) sscanf(buf, "%d", &v);
    return ret;
}

static int recv_hostsend(int fd, int &len, char* src, char* payload) {
    char buf[9999];
    int ret = recv(fd, buf);
    if(ret != -1) sscanf(buf, "%d %s %s", &len, src, payload);
    return ret;
}

static int recv_extersend(int fd, int &len, char* src, char* dst, char* payload) {
    char buf[9999];
    int ret = recv(fd, buf);
    if(ret != -1) sscanf(buf, "%d %s %s %s", &len, src, dst, payload);
    return ret;
}

static void send_pretest(int write_fd, int read_fd, int max_round) {
    int round = 1e9;
    while(round > max_round) {
        send_trigger(write_fd);
        send_ns(write_fd);
        recv_ns(read_fd, round);
        if(round == 1e9) cerr << "GTest: time out when waiting for simulator stable" << endl;
        if(round == 65536 ) {
            cerr << "GTest: too many iterations before being stable";
            break;
        }
    }
}

/* Routing TEST will not use NAT */
TEST_F(Routing, Forward) {
    srand(20231120);

    send_new(write_fd, 6, 0, "0", "0");
    int id, ret;
    ret = recv_new(read_fd, id);
    ASSERT_NE(ret, -1);

    char ips[6][255] = {"10.0.0.0", "10.0.0.1", "10.0.0.2", "10.0.0.3", "10.0.0.4", "10.0.0.5"};
    for(int i = 0; i < 5; i ++) send_addhost(write_fd, id, ips[i]);
    send_pretest(write_fd, read_fd, 2);
    char payload[64], src[256], res_payload[256];
    srand(time(NULL));
    for(int i = 0; i < 63; i ++) payload[i] = rand() % 26 + 'a'; 
    payload[63] = 0;
    srand(20231120);

    /* between any pair of */
    for(int i = 0; i < 5; i ++)
        for(int j = 0; j < 5; j ++) {
            if(i == j) continue;
            send_hostsend(write_fd, ips[i], ips[j], payload);
            recv_hostsend(read_fd, ret, src, res_payload);
            if(ret < 0) output_error_info(ret);
            ASSERT_EQ(ret, 0);
            ASSERT_EQ(strcmp(payload, res_payload), 0);
        }
    
    send_hostsend(write_fd, ips[0], ips[5], payload);
    recv_hostsend(read_fd, ret, src, res_payload);
    if(ret != -2) {
        output_error_info(ret);
        cerr << "GTest: expect sending to controller" << endl;
    }
    ASSERT_EQ(ret, -2);

    send_exit(write_fd);
    int retval = wait_exit(controller_pid);
    EXPECT_GE(retval, 0);
}

TEST_F(Routing, Accessibility) {
    srand(20231120);

    const int num1 = 20;
    const int num2 = 50;

    set<int> edge[2][105];
    int fa[2][105], du[2][105];
    {
        memset(fa, 0, sizeof(fa));
        memset(du, 0, sizeof(du));
        for(int i = 1; i < num1; i ++) {
            fa[0][i] = rand() % i;
            du[0][i] = 1;
            du[0][fa[0][i]] ++;
            edge[0][i].insert(fa[0][i]);
            edge[0][fa[0][i]].insert(i);
        }
        for(int i = 1; i < num2; i ++) {
            fa[1][i] = rand() % i;
            du[1][i] = 1;
            du[1][fa[1][i]] += 1;
            edge[1][i].insert(fa[1][i]);
            edge[1][fa[1][i]].insert(i);
        }
        for(int i = 0; i < 2; i ++) {
            int x = rand() % num2;
            int y = rand() % num2;
            if(x == y || edge[1][x].count(y)) {
                i --;
                continue;
            }
            edge[1][x].insert(y);
            edge[1][y].insert(x);
            du[1][x] ++;
            du[1][y] ++;
        }
    }

    int ids[2][105], max_round = 0;
    for(int i = 0; i < num1; i ++) {
        max_round = max(max_round, du[0][i] + 1);
        send_new(write_fd, 2 + du[0][i], 0, "0", "0");
        int ret = recv_new(read_fd, ids[0][i]);
        ASSERT_NE(ret, -1);
    }
    for(int i = 0; i < num2; i ++) {
        max_round = max(max_round, du[1][i] + 1);
        send_new(write_fd, 2 + du[1][i], 0, "0", "0");
        int ret = recv_new(read_fd, ids[1][i]);
        ASSERT_NE(ret, -1);
    }

    char ips[2][105][255];
    uint32_t base_ip;
    s2ipv4("10.0.0.0", base_ip);
    for(int i = 0; i < num1; i ++) {
        ipv42s(ips[0][i], base_ip);
        base_ip += ip_delta;
    }
    for(int i = 0; i < num2; i ++) {
        ipv42s(ips[1][i], base_ip);
        base_ip += ip_delta;
    }

    for(int i = 0; i < num1; i ++)
        for(auto j : edge[0][i]) {
            if(j >= i) continue;
            send_link(write_fd, ids[0][i], ids[0][j], 1);
        }
    for(int i = 1; i < num2; i ++) 
        for(auto j : edge[1][i]) {
            if(j >= i) continue;
            send_link(write_fd, ids[1][i], ids[1][j], 1);
        }

    send_pretest(write_fd, read_fd, max_round);

    for(int i = 0; i < num1; i ++) send_addhost(write_fd, ids[0][i], ips[0][i]);
    for(int i = 0; i < num2; i ++) send_addhost(write_fd, ids[1][i], ips[1][i]);

    send_pretest(write_fd, read_fd, max_round);
    char payload[64];
    srand(time(NULL));
    for(int i = 0; i < 63; i ++) payload[i] = rand() % 26 + 'a'; 
    payload[63] = 0;
    srand(20231120);

    int ret;
    char src[256], res_payload[256];
    for(int i = 0; i < 10; i ++) {
        int x = rand() % num1, y = rand() % num1;
        while(y == x) y = rand() % num1;
        send_hostsend(write_fd, ips[0][x], ips[0][y], payload);
        recv_hostsend(read_fd, ret, src, res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_GE(ret, 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
        ASSERT_EQ(strcmp(src, ips[0][x]), 0);
    }
    for(int i = 0; i < 10; i ++) {
        int x = rand() % num2, y = rand() % num2;
        while(y == x) y = rand() % num2;
        send_hostsend(write_fd, ips[1][x], ips[1][y], payload);
        recv_hostsend(read_fd, ret, src, res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_GE(ret, 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
    }
    for(int i = 0; i < 10; i ++) {
        int x = rand() % num1, y = rand() % num2;
        send_hostsend(write_fd, ips[0][x], ips[1][y], payload);
        recv_hostsend(read_fd, ret, src, res_payload);
        if(ret != -2) {
            output_error_info(ret);
            cerr << "GTest: expect sending to controller" << endl;
        }
        ASSERT_EQ(ret, -2);
    }

    send_exit(write_fd);
    int retval = wait_exit(controller_pid);
    EXPECT_GE(retval, 0);
}

TEST_F(Routing, StaticOptimal) {
    srand(20231120);

    const int level = 8;
    const int num_pl = 10;
    const int edge_pn = 2;
    const int max_value = 100;
    map<int, int> edge[level][num_pl];
    int du[level][num_pl];
    memset(du, 0 , sizeof(du));

    for(int i = 0; i < level - 1; i ++)
        for(int j = 0; j < num_pl; j ++) 
            for(int k = 0; k < edge_pn; k ++) {
                int x = rand() % num_pl;
                while(edge[i][j].count(x)) x = rand() % num_pl;
                edge[i][j][x] = rand() % max_value + 1;
                du[i][j] ++;
                du[i + 1][x] ++;
            }

    int ids[level][num_pl], max_round = 0;
    char ips[level][num_pl][255];
    uint32_t base_ip;
    s2ipv4("10.0.0.0", base_ip);

    for(int i = 0; i < level; i ++)
        for(int j = 0; j < num_pl; j ++) {
            max_round = max(max_round, du[i][j] + 1);
            send_new(write_fd, 2 + du[i][j], 0, "0", "0");
            int ret = recv_new(read_fd, ids[i][j]);
            EXPECT_NE(ret, -1);
        }
    
    for(int i = 0; i < level - 1; i ++) {
        for(int j = 0; j < num_pl; j ++)
            for(auto p : edge[i][j]) {
                int y = p.first, v = p.second;
                send_link(write_fd, ids[i][j], ids[i + 1][y], v);
            }
        send_pretest(write_fd, read_fd, max_round);
    }

    for(int i = 0; i < level; i ++)
        for(int j = 0; j < num_pl; j ++) {
            ipv42s(ips[i][j], base_ip);
            base_ip += ip_delta;
            send_addhost(write_fd, ids[i][j], ips[i][j]);
        }
    
    send_pretest(write_fd, read_fd, max_round);
    char payload[64];
    srand(time(NULL));
    for(int i = 0; i < 63; i ++) payload[i] = rand() % 26 + 'a'; 
    payload[63] = 0;
    srand(20231120);

    int dis[123][123];
    const int inf = 1e9;
    {
        int max_ids = 0;
        for(int i = 0; i < level; i ++)
        for(int j = 0; j < num_pl; j ++) max_ids = max(max_ids, ids[i][j] + 1);
        for(int i = 0; i < max_ids; i ++)
            for(int j = 0; j < max_ids; j ++) dis[i][j] = inf;
        for(int i = 0; i < max_ids; i ++) dis[i][i] = 0;
        for(int i = 0; i < level - 1; i ++)
            for(int j = 0; j < num_pl; j ++)
                for(auto  p : edge[i][j]) {
                    int y = p.first, v = p.second;
                    dis[ids[i][j]][ids[i + 1][y]] = v;
                    dis[ids[i + 1][y]][ids[i][j]] = v;
                }
        for(int k = 1; k < max_ids; k ++)
        for(int i = 1; i < max_ids; i ++)
        for(int j = 1; j < max_ids; j ++) dis[i][j] = min(dis[i][j], dis[i][k] +  dis[k][j]);
    }

    int ret;
    char src[256], res_payload[256];
    const int query_num = min(100, level * num_pl * (level * num_pl - 1) >> 1);

    for(int i = 0; i < query_num; i ++) {
        int x1 = rand() % level, y1 = rand() % num_pl;
        int x2 = rand() % level, y2 = rand() % num_pl;
        while((x1 == x2) && (y1 == y2)) {
            x2 = rand() % level;
            y2 = rand() % num_pl;
        }
        send_hostsend(write_fd, ips[x1][y1], ips[x2][y2], payload);
        recv_hostsend(read_fd, ret, src, res_payload);
        int ans = dis[ids[x1][y1]][ids[x2][y2]];
        if(ans < inf) { 
            if(ret < 0) output_error_info(ret);
            ASSERT_EQ(ret, ans);
            ASSERT_EQ(strcmp(payload, res_payload), 0);
        }
        else {
            if(ret != -2) {
                output_error_info(ret);
                cerr << "GTest: expect sending to controller" << endl;
            }
            ASSERT_EQ(ret, -2);
        }
    }

    send_exit(write_fd);
    int retval = wait_exit(controller_pid);
    EXPECT_GE(retval, 0);
}

/* add edge on tree */
TEST_F(Routing, DynamicOptimalAdd) {
    srand(20231120);

    const int num = 20;
    map<int, int> edge[123];
    const int max_value = 100;

    int du[123];
    memset(du, 0, sizeof(du));

    for(int i = 1; i < num; i ++) {
        int fa = rand() % i;
        int value = rand() % max_value + 1;
        edge[i][fa] = value;
        edge[fa][i] = value;
        du[i] ++;
        du[fa] ++;
    }

    int ids[123];
    char ips[123][255];
    uint32_t base_ip;
    s2ipv4("10.0.0.0", base_ip);

    for(int i = 0; i < num; i ++) {
        send_new(write_fd, 2 + num, 0, "0", "0");
        int ret = recv_new(read_fd, ids[i]);
        EXPECT_NE(ret, -1);
    }

    for(int i = 0; i < num; i ++)
        for(auto j : edge[i]) {
            if(i <= j.first) continue;
            send_link(write_fd, ids[i], ids[j.first], j.second);
        }

    int max_round = 0;
    for(int i = 0; i < num; i ++) max_round = max(max_round, du[i] + 1);
    send_pretest(write_fd, read_fd, max_round);

    for(int i = 0; i < num; i ++) {
        ipv42s(ips[i], base_ip);
        base_ip += ip_delta;
        send_addhost(write_fd, ids[i], ips[i]);
    }
    
    max_round = 0;
    for(int i = 0; i < num; i ++) max_round = max(max_round, du[i] + 1);
    send_pretest(write_fd, read_fd, max_round);

    char payload[64];
    srand(time(NULL));
    for(int i = 0; i < 63; i ++) payload[i] = rand() % 26 + 'a'; 
    payload[63] = 0;
    srand(20231120);

    const int round = 10;
    const int modify_num = 5;
    const int query_num = 30;

    for(int t = 0; t < round; t ++) {
        for(int r = 0; r < modify_num; r ++) {
            int x = rand() % num;
            int y = rand() % num;
            while(x == y || edge[x].count(y)) {
                x = rand() % num;
                y = rand() % num;
            }
            int value = rand() % max_value + 1;
            edge[x][y] = value;
            edge[y][x] = value;
            send_link(write_fd, ids[x], ids[y], value);
            du[x] ++;
            du[y] ++;
        }
        int max_round = 0;
        for(int i = 0; i < num; i ++) max_round = max(max_round, du[i] + 1);

        send_pretest(write_fd, read_fd, max_round);
        int dis[123][123];
        const int inf = 1e9;
        {
            for(int i = 0; i < num; i ++)
                for(int j = 0; j < num; j ++) dis[i][j] = inf;
            for(int i = 0; i < num; i ++) dis[i][i] = 0;
            for(int i = 0; i < num; i ++)
                for(auto j : edge[i]) dis[i][j.first] = j.second;
            for(int k = 0; k < num; k ++)
            for(int i = 0; i < num; i ++)
            for(int j = 0; j < num; j ++) dis[i][j] = min(dis[i][j], dis[i][k] + dis[k][j]);
        }

        int ret;
        char src[256], res_payload[256];

        for(int r = 0; r < query_num; r ++) {
            int x = rand() % num;
            int y = rand() % num;
            while(x == y) y = rand() % num;
            send_hostsend(write_fd, ips[x], ips[y], payload);
            recv_hostsend(read_fd, ret, src, res_payload);
            int ans = dis[x][y];
            if(ans < inf) {
                if(ret < 0) output_error_info(ret);
                ASSERT_EQ(ret, ans);
            }
            else {
                if(ret != -2) {
                    output_error_info(ret);
                    cerr << "GTest: expect sending to controller" << endl;
                }
                ASSERT_EQ(ret, -2);
            }
            ASSERT_EQ(strcmp(payload, res_payload), 0);
        }
    }

    send_exit(write_fd);
    int retval = wait_exit(controller_pid);
    EXPECT_GE(retval, 0);
}

TEST_F(Routing, DynamicOptimalDel) {
    srand(20231120);

    const int num = 20;
    map<int, int> edge[123];
    const int max_value = 100;

    int du[123];
    memset(du, 0, sizeof(du));

    for(int i = 1; i < num; i ++) {
        int fa = rand() % i;
        int value = rand() % max_value + 1 + max_value;
        edge[i][fa] = value;
        edge[fa][i] = value;
        du[i] ++;
        du[fa] ++;
    }

    const int ap_edge = 50;
    vector<pair<int, int> > ap_edges;

    for(int i = 0; i < ap_edge; i ++) {
        int x = rand() % num;
        int y = rand() % num;
        while(x == y || edge[x].count(y)) {
            x = rand() % num;
            y = rand() % num;
        }
        int value = rand() % max_value + 1;
        edge[x][y] = edge[y][x] = value;
        ap_edges.push_back(make_pair(x, y));
        du[x] ++;
        du[y] ++;
    }
    random_shuffle(ap_edges.begin(), ap_edges.end());

    int ids[123];
    char ips[123][255];
    uint32_t base_ip;
    s2ipv4("10.0.0.0", base_ip);

    for(int i = 0; i < num; i ++) {
        send_new(write_fd, 2 + num, 0, "0", "0");
        int ret = recv_new(read_fd, ids[i]);
        EXPECT_NE(ret, -1);
    }

    for(int i = 0; i < num; i ++)
        for(auto j : edge[i]) {
            if(i <= j.first) continue;
            send_link(write_fd, ids[i], ids[j.first], j.second);
        }
    
    int max_round = 0;
    for(int i = 0; i < num; i ++) max_round = max(max_round, du[i] + 1);
    send_pretest(write_fd, read_fd, max_round);

    for(int i = 0; i < num; i ++) {
        ipv42s(ips[i], base_ip);
        base_ip += ip_delta;
        send_addhost(write_fd, ids[i], ips[i]);
    }

    max_round = 0;
    for(int i = 0; i < num; i ++) max_round = max(max_round, du[i] + 1);
    send_pretest(write_fd, read_fd, max_round);

    char payload[64];
    srand(time(NULL));
    for(int i = 0; i < 63; i ++) payload[i] = rand() % 26 + 'a'; 
    payload[63] = 0;
    srand(20231120);

    const int round = 10;
    const int modify_num = ap_edge / round;
    const int query_num = 30;
    int del_cnt = 0;

    for(int t = 0; t < round; t ++) {
        for(int r = 0; r < modify_num; r ++) {
            pair<int, int> item = ap_edges[del_cnt ++];
            int x = item.first;
            int y = item.second;
            edge[x].erase(y);
            edge[y].erase(x);
            send_cut(write_fd, ids[x], ids[y]);
            du[x] --;
            du[y] --;
        }

        int max_round = 0;
        for(int i = 0; i < num; i ++) max_round = max(max_round, du[i] + 1);

        send_pretest(write_fd, read_fd, max_round);
        int dis[123][123];
        const int inf = 1e9;
        {
            for(int i = 0; i < num; i ++)
                for(int j = 0; j < num; j ++) dis[i][j] = inf;
            for(int i = 0; i < num; i ++) dis[i][i] = 0;
            for(int i = 0; i < num; i ++)
                for(auto j : edge[i]) dis[i][j.first] = j.second;
            for(int k = 0; k < num; k ++)
            for(int i = 0; i < num; i ++)
            for(int j = 0; j < num; j ++) dis[i][j] = min(dis[i][j], dis[i][k] + dis[k][j]);
        }

        int ret;
        char src[256], res_payload[256];

        for(int r = 0; r < query_num; r ++) {
            int x = rand() % num;
            int y = rand() % num;
            while(x == y) y = rand() % num;
            send_hostsend(write_fd, ips[x], ips[y], payload);
            recv_hostsend(read_fd, ret, src, res_payload);
            int ans = dis[x][y];
            if(ans < inf) {
                if(ret < 0) output_error_info(ret);
                ASSERT_EQ(ret, ans);
            }
            else {
                if(ret != -2) {
                    output_error_info(ret);
                    cerr << "GTest: expect sending to controller" << endl;
                }
                ASSERT_EQ(ret, -2);
            }
            ASSERT_EQ(strcmp(payload, res_payload), 0);
        }
    }

    send_exit(write_fd);
    int retval = wait_exit(controller_pid);
    EXPECT_GE(retval, 0);
}

TEST_F(Routing, DynamicOptimalMix) {
    srand(20231120);

    const int num = 50;
    map<int, int> edge[123];
    const int max_value = 100;

    int du[123];
    memset(du, 0, sizeof(du));

    vector<pair<int, int> > ori_edges;
    for(int i = 1; i < num; i ++) {
        int fa = rand() % i;
        int value = rand() % max_value + 1 + max_value;
        edge[i][fa] = value;
        edge[fa][i] = value;
        du[i] ++;
        du[fa] ++;
        ori_edges.push_back(make_pair(i, fa));
    }

    const int ap_edge = 200;
    vector<pair<int, int> > ap_edges;

    for(int i = 0; i < ap_edge; i ++) {
        int x = rand() % num;
        int y = rand() % num;
        while(x == y || edge[x].count(y)) {
            x = rand() % num;
            y = rand() % num;
        }
        int value = rand() % max_value + 1;
        edge[x][y] = edge[y][x] = value;
        ap_edges.push_back(make_pair(x,y));
        du[x] ++;
        du[y] ++;
    }
    random_shuffle(ap_edges.begin(), ap_edges.end());

    int ids[123];
    char ips[123][255];
    uint32_t base_ip;
    s2ipv4("10.0.0.0", base_ip);

    for(int i = 0; i < num; i ++) {
        send_new(write_fd, 2 + num, 0, "0", "0");
        int ret = recv_new(read_fd, ids[i]);
        EXPECT_NE(ret, -1);
    }

    for(int i = 0; i < num; i ++)
        for(auto j : edge[i]) {
            if(i <= j.first) continue;
            send_link(write_fd, ids[i], ids[j.first], j.second);
        }
    
    int max_round = 0;
    for(int i = 0; i < num; i ++) max_round = max(max_round, du[i] + 1);
    send_pretest(write_fd, read_fd, max_round);

    for(int i = 0; i < num; i ++) {
        ipv42s(ips[i], base_ip);
        base_ip += ip_delta;
        send_addhost(write_fd, ids[i], ips[i]);
    }

    max_round = 0;
    for(int i = 0; i < num; i ++) max_round = max(max_round, du[i] + 1);
    send_pretest(write_fd, read_fd, max_round);

    char payload[64];
    srand(time(NULL));
    for(int i = 0; i < 63; i ++) payload[i] = rand() % 26 + 'a'; 
    payload[63] = 0;
    srand(20231120);

    const int round = 25;
    const int del_num = ap_edge / round;
    const int add_num = del_num >> 1;
    const int change_num = 2;
    const int query_num = 30;
    
    for(int t = 0; t < round; t ++) {
        for(int r = 0; r < del_num; r ++) {
            pair<int, int> item = *(ap_edges.end() - 1);
            ap_edges.pop_back();
            int x = item.first;
            int y = item.second;
            edge[x].erase(y);
            edge[y].erase(x);
            send_cut(write_fd, ids[x], ids[y]);
            du[x] --;
            du[y] --;
        }
        for(int r = 0; r < add_num; r ++) {
            int x = rand() % num;
            int y = rand() % num;
            while(x == y || edge[x].count(y)) {
                x = rand() % num;
                y = rand() % num;
            }
            int value = rand() % max_value + 1;
            edge[x][y] = edge[y][x] = value;
            ap_edges.push_back(make_pair(x, y));
            send_link(write_fd, ids[x], ids[y], value);
            du[x] ++;
            du[y] ++;
        }
        random_shuffle(ap_edges.begin(), ap_edges.end());

        for(int r = 0; r < change_num; r ++) {
            int select = rand() & 1;
            int x, y, v = rand() % (max_value << 1) + 1;
            if(select) {
                int idx = rand() % ori_edges.size();
                x = ori_edges[idx].first;
                y = ori_edges[idx].second;
            }
            else {
                int idx = rand() % ap_edges.size();
                x = ap_edges[idx].first;
                y = ap_edges[idx].second;
            }
            edge[x][y] = edge[y][x] = v;
            send_weight(write_fd, ids[x], ids[y], v);
        }

        int max_round = 0;
        for(int i = 0; i < num; i ++) max_round = max(max_round, du[i] + 1);

        send_pretest(write_fd, read_fd, max_round);
        int dis[123][123];
        const int inf = 1e9;
        {
            for(int i = 0; i < num; i ++)
                for(int j = 0; j < num; j ++) dis[i][j] = inf;
            for(int i = 0; i < num; i ++) dis[i][i] = 0;
            for(int i = 0; i < num; i ++)
                for(auto j : edge[i]) dis[i][j.first] = j.second;
            for(int k = 0; k < num; k ++)
            for(int i = 0; i < num; i ++)
            for(int j = 0; j < num; j ++) dis[i][j] = min(dis[i][j], dis[i][k] + dis[k][j]);
        }

        int ret;
        char src[256], res_payload[256];

        for(int r = 0; r < query_num; r ++) {
            int x = rand() % num;
            int y = rand() % num;
            while(x == y) y = rand() % num;
            send_hostsend(write_fd, ips[x], ips[y], payload);
            recv_hostsend(read_fd, ret, src, res_payload);
            int ans = dis[x][y];
            if(ans < inf) {
                if(ret < 0) output_error_info(ret);
                ASSERT_EQ(ret, ans);
            }
            else {
                if(ret != -2) {
                    output_error_info(ret);
                    cerr << "GTest: expect sending to controller" << endl;
                }
                ASSERT_EQ(ret, -2);
            }
            ASSERT_EQ(strcmp(payload, res_payload), 0);
        }
    }

    send_exit(write_fd);
    int retval = wait_exit(controller_pid);
    EXPECT_GE(retval, 0);
}

/* NAT TEST will not check Routing Optimal */
/* using only one router to test */
TEST_F(NAT, Basic) {
    srand(20231120);

    char external_addr[256] = "22.11.21.0/24";
    char available_addr[256] = "21.11.22.0/24";
    send_new(write_fd, 7, 5, external_addr, available_addr);
    int id, ret;
    ret = recv_new(read_fd, id);
    EXPECT_NE(ret, -1);

    char ips[6][255] = {"10.0.0.0", "10.0.0.1", "10.0.0.2", "10.0.0.3", "10.0.0.4"};
    char avail_ips[6][255];
    char exter_ips[2][255] = {"22.11.21.3", "22.11.21.253"};
    for(int i = 0; i < 5; i ++) send_addhost(write_fd, id, ips[i]);
    send_pretest(write_fd, read_fd, 2);
    char payload[64], res_src[256], res_dst[256], res_payload[256];
    srand(time(NULL));
    for(int i = 0; i < 63; i ++) payload[i] = rand() % 26 + 'a'; 
    payload[63] = 0;
    srand(20231120);

    for(int i = 0; i < 5; i ++) {
        send_hostsend(write_fd, ips[i], exter_ips[i % 2], payload);
        recv_hostsend(read_fd, ret, avail_ips[i], res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
        ASSERT_EQ(is_sub_addr(avail_ips[i], available_addr), true);
    }
    for(int i = 0; i < 5; i ++) {
        send_extersend(write_fd, id, exter_ips[i % 2], avail_ips[i], payload);
        recv_extersend(read_fd, ret, res_src, res_dst, res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(strcmp(res_dst, ips[i]), 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
    }
    for(int i = 0; i < 5; i ++) {
        send_hostsend(write_fd, ips[i], exter_ips[i % 2], payload);
        recv_hostsend(read_fd, ret, res_src, res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(strcmp(res_src, avail_ips[i]), 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
    }

    send_extersend(write_fd, id, exter_ips[0], "10.0.0.0", payload);
    recv_extersend(read_fd, ret, res_src, res_dst, res_payload);
    if(ret != -4) {
        output_error_info(ret);
        cerr << "GTest: expect dropping packet" << endl;
    }
    ASSERT_EQ(ret, -4); // drop

    send_exit(write_fd);
    int retval = wait_exit(controller_pid);
    EXPECT_GE(retval, 0);
}

/* considering release command */
TEST_F(NAT, Dynamic) {
    srand(20231120);

    char external_addr[256] = "22.11.21.0/24";
    char available_addr[256] = "21.11.22.0/24";
    send_new(write_fd, 263, 5, external_addr, available_addr);
    int id, ret;
    ret = recv_new(read_fd, id);
    EXPECT_NE(ret, -1);

    char ips[256][255];
    uint32_t base_ip;
    s2ipv4("10.0.0.0", base_ip);
    for(int i = 0; i < 256; i ++) {
        ipv42s(ips[i], base_ip);
        base_ip += ip_delta;
    }
    char mips[6][255] = {"10.0.1.0", "10.0.1.1", "10.0.1.2", "10.0.1.3", "10.0.1.4"};
    char avail_ips[256][255];
    char exter_ip[255] = "22.11.21.3";

    for(int i = 0; i < 256; i ++) send_addhost(write_fd, id, ips[i]);
    for(int i = 0; i < 5; i ++) send_addhost(write_fd, id, mips[i]);
    send_pretest(write_fd, read_fd, 2);
    char payload[64], res_src[256], res_dst[256], res_payload[256];
    srand(time(NULL));
    for(int i = 0; i < 63; i ++) payload[i] = rand() % 26 + 'a'; 
    payload[63] = 0;
    srand(20231120);

    set<string> unique_avail;
    for(int i = 0; i < 256; i ++) {
        send_hostsend(write_fd, ips[i], exter_ip, payload);
        recv_hostsend(read_fd, ret, avail_ips[i], res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
        ASSERT_EQ(is_sub_addr(avail_ips[i], available_addr), true);
        ASSERT_EQ(unique_avail.count(string(avail_ips[i])), 0);
        unique_avail.insert(string(avail_ips[i]));
    }
    for(int i = 0; i < 256; i ++) {
        send_extersend(write_fd, id, exter_ip, avail_ips[i], payload);
        recv_extersend(read_fd, ret, res_src, res_dst, res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(strcmp(res_dst, ips[i]), 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
    }
    for(int i = 0; i < 256; i ++) {
        send_hostsend(write_fd, ips[i], exter_ip, payload);
        recv_hostsend(read_fd, ret, res_src, res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(strcmp(res_src, avail_ips[i]), 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
    }

    for(int i = 0; i < 5; i ++) {
        send_hostsend(write_fd, mips[i], exter_ip, payload);
        recv_hostsend(read_fd, ret, res_src, res_payload);
        if(ret != -4) {
            output_error_info(ret);
            cerr << "GTest: expect dropping packet" << endl;
        }
        ASSERT_EQ(ret, -4);
    }

    send_release(write_fd, id, ips[0]);
    send_pretest(write_fd, read_fd, 2);
    send_extersend(write_fd, id, exter_ip, avail_ips[0], payload);
    recv_extersend(read_fd, ret, res_src, res_dst, res_payload);
    if(ret != -4) {
        output_error_info(ret);
        cerr << "GTest: expect dropping packet" << endl;
    }
    ASSERT_EQ(ret, -4);
    send_hostsend(write_fd, mips[0], exter_ip, payload);
    recv_hostsend(read_fd, ret, res_src, res_payload);
    if(ret < 0) output_error_info(ret);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(strcmp(payload, res_payload), 0);
    ASSERT_EQ(strcmp(res_src, avail_ips[0]), 0);
    send_extersend(write_fd, id, exter_ip, avail_ips[0], payload);
    recv_extersend(read_fd, ret, res_src, res_dst, res_payload);
    if(ret < 0) output_error_info(ret);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(strcmp(res_dst, mips[0]), 0);
    ASSERT_EQ(strcmp(payload, res_payload), 0);
    send_hostsend(write_fd, mips[2], exter_ip, payload);
    recv_hostsend(read_fd, ret, res_src, res_payload);
    if(ret != -4) {
        output_error_info(ret);
        cerr << "GTest: expect dropping packet" << endl;
    }
    ASSERT_EQ(ret, -4);

    send_exit(write_fd);
    int retval = wait_exit(controller_pid);
    EXPECT_GE(retval, 0);
}

/* FireWall TEST only check block and un block commands*/
/* test internal block */
TEST_F(FireWall, Block1){
    srand(20231120);
    send_new(write_fd, 6, 0, "0", "0");
    int id, ret;
    ret = recv_new(read_fd, id);
    EXPECT_NE(ret, -1);

    char ips[6][255] = {"10.0.0.0", "10.0.0.1", "10.0.0.2", "10.0.0.3", "10.0.0.4", "10.0.1.5"};
    for(int i = 0; i < 5; i ++) send_addhost(write_fd, id, ips[i]);
    send_pretest(write_fd, read_fd, 2);
    char payload[64], res_src[256], res_payload[256];
    srand(time(NULL));
    for(int i = 0; i < 63; i ++) payload[i] = rand() % 26 + 'a'; 
    payload[63] = 0;
    srand(20231120);

    send_blockaddr(write_fd, id, ips[0]);
    send_pretest(write_fd, read_fd, 2);
    for(int i = 1; i < 5; i ++) {
        send_hostsend(write_fd, ips[i], ips[0], payload);
        recv_hostsend(read_fd, ret, res_src, res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
    }
    for(int i = 1; i < 5; i ++) {
        send_hostsend(write_fd, ips[0], ips[i], payload);
        recv_hostsend(read_fd, ret, res_src, res_payload);
        if(ret!=-8) {
            output_error_info(ret);
            cerr << "GTest: expect dropping packet" << endl;
        }
        ASSERT_EQ(ret, -8);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
    }
    // reblock a blocked ip should not crash the router
    send_blockaddr(write_fd, id, ips[0]);
    send_blockaddr(write_fd, id, ips[0]);
    send_pretest(write_fd, read_fd, 2);
    // unblock a normal ip should not crash the router
    send_unblockaddr(write_fd, id, ips[1]);
    // block a non-exist ip should not crash the router
    send_unblockaddr(write_fd, id, ips[5]);
    send_unblockaddr(write_fd, id, ips[0]);
    send_pretest(write_fd, read_fd, 3);
    for(int i = 1; i < 5; i ++) {
        send_hostsend(write_fd, ips[0], ips[i], payload);
        recv_hostsend(read_fd, ret, res_src, res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
    }

    send_exit(write_fd);
    int retval = wait_exit(controller_pid);
    EXPECT_GE(retval, 0);
}

/* test external block with NAT */
TEST_F(FireWall, Block2){
    srand(20231120);

    char external_addr[256] = "22.11.21.0/24";
    char available_addr[256] = "21.11.22.0/24";
    send_new(write_fd, 7, 5, external_addr, available_addr);
    int id, ret;
    ret = recv_new(read_fd, id);
    EXPECT_NE(ret, -1);

    char ips[6][255] = {"10.0.0.0", "10.0.0.1", "10.0.0.2", "10.0.0.3", "10.0.0.4"};
    char avail_ips[6][255];
    char tmp_ip[255];
    char exter_ips[2][255] = {"22.11.21.3", "22.11.21.253"};
    for(int i = 0; i < 5; i ++) send_addhost(write_fd, id, ips[i]);
    send_pretest(write_fd, read_fd, 2);
    char payload[64], res_src[256], res_dst[256], res_payload[256];
    srand(time(NULL));
    for(int i = 0; i < 63; i ++) payload[i] = rand() % 26 + 'a'; 
    payload[63] = 0;
    srand(20231120);
    
    // internal send to externel
    for(int i = 0; i < 5; i ++) {
        send_hostsend(write_fd, ips[i], exter_ips[i % 2], payload);
        recv_hostsend(read_fd, ret, avail_ips[i], res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
        ASSERT_EQ(is_sub_addr(avail_ips[i], available_addr), true);
    }
    for(int i = 0;i<5;++i){
        send_blockaddr(write_fd, id, ips[i]);
    }
    send_pretest(write_fd, read_fd, 5);
    for(int i = 0; i < 5; i ++) {
        send_hostsend(write_fd, ips[i], exter_ips[i % 2], payload);
        recv_hostsend(read_fd, ret, tmp_ip, res_payload);
        if(ret != -8) output_error_info(ret);
        ASSERT_EQ(ret, -8);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
        ASSERT_EQ(strcmp(tmp_ip, ips[i]), 0);
    }
    send_unblockaddr(write_fd, id, ips[0]);
    send_pretest(write_fd, read_fd, 2);
    send_hostsend(write_fd, ips[0], exter_ips[0], payload);
    recv_hostsend(read_fd, ret, tmp_ip, res_payload);
    if(ret < 0) output_error_info(ret);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(strcmp(payload, res_payload), 0);
    ASSERT_EQ(is_sub_addr(tmp_ip, available_addr), true);
    
    // external send to internal
    for(int i = 0; i < 5; i ++) {
        send_extersend(write_fd, id, exter_ips[i % 2], avail_ips[i], payload);
        recv_extersend(read_fd, ret, res_src, res_dst, res_payload);
        if(ret < 0) output_error_info(ret);
        ASSERT_EQ(ret, 0);
        ASSERT_EQ(strcmp(res_dst, ips[i]), 0);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
    }
    for(int j = 0;j<2;++j){
        send_blockaddr(write_fd, id, exter_ips[j]);
    }
    send_pretest(write_fd, read_fd, 5);
    for(int i = 0; i < 5; i ++) {
        send_extersend(write_fd, id, exter_ips[i % 2], avail_ips[i], payload);
        recv_extersend(read_fd, ret, res_src, res_dst, res_payload);
        if(ret != -8) output_error_info(ret);
        ASSERT_EQ(ret, -8);
        ASSERT_EQ(strcmp(payload, res_payload), 0);
    }
    send_unblockaddr(write_fd, id, exter_ips[0]);
    send_pretest(write_fd, read_fd, 2);
    send_extersend(write_fd, id, exter_ips[0], avail_ips[0], payload);
    recv_extersend(read_fd, ret, res_src, res_dst, res_payload);
    if(ret < 0) output_error_info(ret);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(strcmp(payload, res_payload), 0);
    ASSERT_EQ(strcmp(res_dst, ips[0]), 0);

    send_exit(write_fd);
    int retval = wait_exit(controller_pid);
    EXPECT_GE(retval, 0);
}

/* Mix Routing and NAT together */
/* No modification */
TEST_F(General, Static) {
    srand(20231121);

    const int level = 8;
    const int num_pl = 6;
    const int edge_pn = 2;
    const int max_value = 100;
    map<int, int> edge[level][num_pl];
    int du[level][num_pl];
    memset(du, 0, sizeof(du));

    for(int i = 0; i < level - 1; i ++)
        for(int j = 0; j < num_pl; j ++)
            for(int k = 0; k < edge_pn; k ++) {
                int x = rand() % num_pl;
                while(edge[i][j].count(x)) x = rand() % num_pl;
                edge[i][j][x] = rand() % max_value + 1;
                du[i][j] ++;
                du[i + 1][x] ++;
            }

    int ids[level][num_pl], max_round = 0;
    char ips[level][num_pl][255];
    char external_addr[num_pl][255], available_addr[num_pl][255];
    char exter_addr[num_pl][255];
    uint32_t base_ip;
    s2ipv4("22.11.21.0", base_ip);
    for(int i = 0; i < num_pl; i ++) {
        ipv42s(external_addr[i], base_ip);
        strcat(external_addr[i], "/25");
        ipv42s(exter_addr[i], base_ip + ip_delta * 6);
        base_ip += ip_delta2;
    }
    s2ipv4("21.11.22.0", base_ip);
    for(int i = 0; i < num_pl; i ++) {
        ipv42s(available_addr[i], base_ip);
        strcat(available_addr[i], "/25");
        base_ip += ip_delta2;
    }
    s2ipv4("10.0.0.0", base_ip);
    for(int i = 0; i < level; i ++)
        for(int j = 0; j < num_pl; j ++) {
            ipv42s(ips[i][j], base_ip);
            base_ip += ip_delta;
        }

    for(int i = 0; i < level; i ++)
        for(int j = 0; j < num_pl; j ++) {
            max_round = max(max_round, du[i][j] + 1);
            if(i == 0) send_new(write_fd, 3 + du[i][j], 3, external_addr[j], available_addr[j]);
            else send_new(write_fd, 2 + du[i][j], 0, "0", "0");
            int ret = recv_new(read_fd, ids[i][j]);
            EXPECT_NE(ret, -1);
        }

    for(int i = 0; i < level; i ++)
        for(int j = 0; j < num_pl; j ++)
            send_addhost(write_fd, ids[i][j], ips[i][j]);

    send_pretest(write_fd, read_fd, max_round);

    for(int i = 0; i < level - 1; i ++) {
        for(int j = 0; j < num_pl; j ++)
            for(auto p : edge[i][j]) {
                int y = p.first, v = p.second;
                send_link(write_fd, ids[i][j], ids[i + 1][y], v);
                send_pretest(write_fd, read_fd, max_round);
            }
    }

    char payload[64];
    srand(time(NULL));
    for(int i = 0; i < 63; i ++) payload[i] = rand() % 26 + 'a'; 
    payload[63] = 0;
    srand(20231121);

    int dis[123][123];
    const int inf = 1e9;
    {
        int max_ids = 0;
        for(int i = 0; i < level; i ++)
        for(int j = 0; j < num_pl; j ++) max_ids = max(max_ids, ids[i][j] + 1);
        for(int i = 0; i < max_ids; i ++)
            for(int j = 0; j < max_ids; j ++) dis[i][j] = inf;
        for(int i = 0; i < max_ids; i ++) dis[i][i] = 0;
        for(int i = 0; i < level - 1; i ++)
            for(int j = 0; j < num_pl; j ++)
                for(auto  p : edge[i][j]) {
                    int y = p.first, v = p.second;
                    dis[ids[i][j]][ids[i + 1][y]] = v;
                    dis[ids[i + 1][y]][ids[i][j]] = v;
                }
        for(int k = 1; k < max_ids; k ++)
        for(int i = 1; i < max_ids; i ++)
        for(int j = 1; j < max_ids; j ++) dis[i][j] = min(dis[i][j], dis[i][k] +  dis[k][j]);
    }

    int ret;
    char res_src[256], res_dst[256], res_payload[256];
    const int query_num = min(500, level * num_pl * (level * num_pl - 1) >> 1);

    for(int i = 0; i < query_num; i ++) {
        int x1 = rand() % level, y1 = rand() % num_pl;
        int x2 = rand() % level, y2 = rand() % num_pl;
        while((x1 == x2) && (y1 == y2)) {
            x2 = rand() % level;
            y2 = rand() % num_pl;
        }
        send_hostsend(write_fd, ips[x1][y1], ips[x2][y2], payload);
        recv_hostsend(read_fd, ret, res_src, res_payload);
        int ans = dis[ids[x1][y1]][ids[x2][y2]];
        if(ans < inf) {
            if(ret < 0) output_error_info(ret);
            ASSERT_EQ(ret, ans);
            ASSERT_EQ(strcmp(payload, res_payload), 0);
        }
        else {
            if(ret != -2 && ret != -4) {
                output_error_info(ret);
                cerr << "GTest: expect sending to controller" << endl;
            }
            ASSERT_EQ(ret, -2);
        }
    }

    char alloc_addr[level][num_pl][num_pl][255];

    set<string> unique_avail[num_pl];
    for(int i = 0; i < level; i ++)
    for(int j = 0; j < num_pl; j ++)
    for(int k = 0; k < num_pl; k ++) {
        send_hostsend(write_fd, ips[i][j], exter_addr[k], payload);
        recv_hostsend(read_fd, ret, alloc_addr[i][j][k], res_payload);
        int ans = dis[ids[i][j]][ids[0][k]];
        if(ans < inf) {
            if(ret < 0) output_error_info(ret);
            ASSERT_EQ(ret, ans);
            ASSERT_EQ(strcmp(payload, res_payload), 0);
            ASSERT_EQ(is_sub_addr(alloc_addr[i][j][k], available_addr[k]), true);
            ASSERT_EQ(unique_avail[k].count(string(alloc_addr[i][j][k])), 0);
            unique_avail[k].insert(string(alloc_addr[i][j][k]));
        }
        else {
            if(ret != -2) {
                output_error_info(ret);
                cerr << "GTest: expect sending to controller" << endl;
            }
            ASSERT_EQ(ret, -2);
        }
    }
    for(int i = 0; i < level; i ++)
    for(int j = 0; j < num_pl; j ++)
    for(int k = 0; k < num_pl; k ++) {
        send_extersend(write_fd, ids[0][k], exter_addr[k], alloc_addr[i][j][k], payload);
        recv_extersend(read_fd, ret, res_src, res_dst, res_payload);
        int ans = dis[ids[i][j]][ids[0][k]];
        if(ans < inf) {
            if(ret < 0) output_error_info(ret);
            ASSERT_EQ(ret, ans);
            ASSERT_EQ(strcmp(res_dst, ips[i][j]), 0);
            ASSERT_EQ(strcmp(payload, res_payload), 0);
        }
        else {
            if(ret != -2) {
                output_error_info(ret);
                cerr << "GTest: expect sending to controller" << endl;
            }
            ASSERT_EQ(ret, -2);
        }
    }

    send_exit(write_fd);
    int retval = wait_exit(controller_pid);
    EXPECT_GE(retval, 0);
}

/* Not available until deadline */
//TEST_F(General, Dynamic1) {
//}

/* Not available until deadline */
//TEST_F(General, Dynamic2) {
//}

int _tmain(int argc, wchar_t* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}