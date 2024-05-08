#include "router_prototype.h"
#include "util.h"
#include <stdint.h>
#include <set>
#include <map>
#include <vector>
#include <string>

struct simulator_packet_t
{
    uint32_t src;
    uint32_t dst;
    uint8_t type;
    uint16_t length;
    char payload[MAX_PAYLOAD_LENGTH];

    void print()
    {
        struct in_addr inAddr;
        inAddr.s_addr = htonl(src);
        char *str = inet_ntoa(inAddr);
        if (type == 1)
            LOG_DEBUG("    src: %s\n", str);
        inAddr.s_addr = htonl(dst);
        str = inet_ntoa(inAddr);
        if (type == 1)
            LOG_DEBUG("    dst: %s\n", str);
        LOG_DEBUG("    type: %d\n", type);
        LOG_DEBUG("    length: %d\n", length);
        if (type == 1)
            LOG_DEBUG("    payload: %s\n", payload);
    }
};

struct DV
{
    // 唯一确定一个叶子结点
    // 如果是 (0, 0) 则表示还没有得知这个叶子结点
    // 其中 first 为叶子结点（包括外网和 hosh ）的地址，second 为子网所占据的位数，一般是8
    leaf_node_t leaf_node[MAX_LEAF_NODE_NUM];
    // 距离叶子结点的距离
    int leaf_node_dis[MAX_LEAF_NODE_NUM];
    // 下一个新的叶子结点的下标位置
    int new_leaf_node_pos;

    /**
     * @brief 距离向量类
     * @note 本类用于存储距离向量，其中包括叶子结点的地址和距离
     * @note 本类还包括一个新的叶子结点的下标位置，用于在更新距离向量时添加新的叶子结点
     * @author LIANG Yutong
     * @date 12/17/2023
     */
    DV()
    {
        for (int i = 0; i < MAX_LEAF_NODE_NUM; ++i)
        {
            leaf_node[i] = std::make_pair(0, 0);
            leaf_node_dis[i] = 10000000 + i;
        }
        new_leaf_node_pos = 0;
    }

    /**
     * @brief 重载等号运算符
     * @note 本函数用于判断两个距离向量是否相等，如果相等则返回1，否则返回0
     * @author LIANG Yutong
     * @date 12/17/2023
     */
    bool operator==(const DV &A) const
    {
        for (int i = 0; i < MAX_LEAF_NODE_NUM; ++i)
        {
            if (leaf_node[i] != A.leaf_node[i])
                return 0;
            if (leaf_node_dis[i] != A.leaf_node_dis[i])
                return 0;
        }
        return 1;
    }

    /**
     * @brief 打印距离向量
     * @note 本函数用于调试，会打印距离向量中所有的叶子结点的地址（点分十进制）和距离，如果叶子结点的地址为0，则表示还没有得知这个叶子结点
     * @author LIANG Yutong
     * @date 12/17/2023
     */
    void print_dv()
    {
        for (int i = 0; i < MAX_LEAF_NODE_NUM; ++i)
        {
            if (leaf_node[i].first == 0 && leaf_node[i].second == 0)
                continue;
            struct in_addr inAddr;
            inAddr.s_addr = htonl(leaf_node[i].first);
            char *str = inet_ntoa(inAddr);
            LOG_DEBUG("    leaf_node[%d]: %s/%d, dis=%d\n", i, str, leaf_node[i].second, leaf_node_dis[i]);
        }
        LOG_DEBUG("    new_leaf_node_pos: %d\n", new_leaf_node_pos);
    }
};

class Router : public RouterBase
{
private:
    int port_num;

    // 本机可用的公网地址
    leaf_node_t available;
    // 储存本机拥有的公网IP是否已经被占用，如果被占用则为true，数组长度由available_addr_subnet_mask决定
    std::set<uint32_t> available_addr;
    // 公网IP和内网IP的映射，key为公网IP，value为内网IP
    std::map<uint32_t, uint32_t> nat_table;
    // 公网IP和内网IP的映射，key为内网IP，value为公网IP
    std::map<uint32_t, uint32_t> nat_table_reverse;

    // 本机的距离向量
    DV local_dv;
    // 下一跳的端口号，用来进行毒性逆转，其中 next_node_port_num[i] 表示到达叶子结点 i 的下一跳的端口号
    // 如果 next_node_port_num[i] 为 0，则表示没有下一跳
    std::map<leaf_node_t, int> next_node_port_num;
    // 距离邻接表，key为port，value为这个port的对应的路由器的距离
    std::map<int, int> dis;
    // 别的 router 的距离向量，key为port，value为这个port的对应的路由器的距离向量，自己不用修改，存储别的路由器发来的距离向量即可
    std::map<int, DV> dv_table;

    // 如果本机与 host 相邻，key 是 host，value 是本机连到 host 的端口号
    std::map<leaf_node_t, int> host_port;
    // 如果本机与 host 相邻，本机连到 host 的地址
    std::set<leaf_node_t> host;

    // 公网端口号，如果是 0 则表示本机没有外网
    int external_port;
    // 公网地址，如果是 0 则表示本机没有外网
    leaf_node_t external;

    // 被block的地址
    std::set<uint32_t> block;

public:
    /**
     * @brief 构造函数
     * @note 本函数会初始化所有的变量，包括nat表、距离向量表、下一跳端口号等
     * @note 所有的变量都会被初始化为0
     * @author LIANG Yutong
     * @date 12/17/2023
     */
    Router()
    {
        // 初始化外网信息
        port_num = 0;
        external_port = 0;
        external = std::make_pair(0, 0);
        available = std::make_pair(0, 0);
        // 初始化nat表
        available_addr.clear();
        nat_table.clear();
        nat_table_reverse.clear();
        // 初始化host信息
        host_port.clear();
        host.clear();
        // 初始化下一跳端口号，因为端口号从1开始，所以0表示没有下一跳
        next_node_port_num.clear();
        // 初始化距离向量表
        dis.clear();
        dv_table.clear();
    }

    void router_init(int port_num, int external_port, char *external_addr, char *available_addr);
    int router(int in_port, char *packet);

    /**
     * @brief 更新本机的距离向量
     * @note 本函数会更新 local_dv 和 next_node_port_num
     * @author LIANG Yutong
     * @date 12/17/2023
     */
    void update_local_dv();

    /**
     * @brief 获取毒性逆转的距离向量
     * @note 本函数会返回一个距离向量，其中下一跳端口号为 port_num 的距离会被设置为 0x3f3f3f3f
     * @param port_num 下一跳端口号
     * @return DV 毒性逆转的距离向量
     * @author LIANG Yutong
     * @date 12/17/2023
     */
    DV get_poison_reversed_dv(int port_num);
};

/**
 * @brief 构造一个packet
 * @note 本函数会构造一个packet，其中包括源地址、目的地址、类型、长度、payload
 * @param[out] packet 用于存储packet的数组
 * @param[in] src 源地址
 * @param[in] dst 目的地址
 * @param[in] type 类型
 * @param[in] length 负载长度
 * @param[in] payload 负载
 * @author LIANG Yutong
 * @date 12/17/2023
 */
void make_packet(simulator_packet_t *packet, uint32_t src, uint32_t dst, uint8_t type, uint16_t length, char *payload);

/**
 * @brief 将一个leaf_node转换为字符串
 * @param[in] leaf_node 要转换的 leaf_node
 * @return std::string 转换后的字符串，格式为 "address/subnet_mask"，其中 address 为点分十进制，subnet_mask 为十进制数，范围为 0~32，表示子网所占据的位数，一般是8
 * @author LIANG Yutong
 * @date 12/17/2023
 */
std::string leaf_node_to_string(leaf_node_t leaf_node);