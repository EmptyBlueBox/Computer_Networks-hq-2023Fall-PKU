#include "router.h"
#include <assert.h>
#include <string>

RouterBase *create_router_object()
{
    return new Router;
}

void Router::router_init(int _port_num, int _external_port, char *_external_addr, char *_available_addr)
{
    LOG_DEBUG("into router_init()\n");
    port_num = _port_num;
    external_port = _external_port;
    if (external_port)
    {
        parse_ip_addr(_external_addr, &external);
        parse_ip_addr(_available_addr, &available);
        uint32_t l_ip = available.first, r_ip = available.first + (1u << available.second) - 1u;
        for (uint32_t i = l_ip; i <= r_ip; ++i)
            available_addr.insert(i);
    }
    if (external_port)
    {
        local_dv.leaf_node[0] = external;
        local_dv.leaf_node_dis[0] = 0;
        local_dv.new_leaf_node_pos = 1;
        next_node_port_num[external] = external_port;
        dis[external_port] = 0;
        LOG_DEBUG("    local_dv:\n");
        local_dv.print_dv();
    }
    LOG_DEBUG("    port_num: %d, external_port: %d\n", port_num, external_port);
    LOG_DEBUG("exit router_init()\n");
    return;
}

int Router::router(int in_port, char *packet)
{
    LOG_DEBUG("into router()\n");

    uint32_t src = ntohl(((simulator_packet_t *)packet)->src);
    uint32_t dst = ntohl(((simulator_packet_t *)packet)->dst);
    int packet_type = ((simulator_packet_t *)packet)->type;
    int packet_length = ntohs(((simulator_packet_t *)packet)->length);
    ((simulator_packet_t *)packet)->print();

    // 对于所有的报文都要进行如下处理
    // 1. 被block的报文，丢弃
    // 2. 来自公网的报文，进行NAT
    if (block.find(src) != block.end())
    {
        LOG_DEBUG("blocked\n");
        LOG_DEBUG("exit router()\n\n");
        return -1;
    }
    if (external_port && in_port == external_port)
    {
        LOG_DEBUG("from external\n");
        // 如果这个来自公网的报文的目的地址是已经被分配的地址，就进行NAT
        if (nat_table[dst])
        {
            assert(available_addr.find(dst) == available_addr.end());
            ((simulator_packet_t *)packet)->dst = htonl(nat_table[dst]);
            dst = nat_table[dst]; // 包的dst变了，局部变量dst也要变
            // 不用转发，等待后续处理
        }
        // 如果这个来自公网的报文的目的地址是没有被分配的地址，就丢弃
        else
        {
            LOG_DEBUG("exit router()\n\n");
            return -1;
        }
    }

    if (packet_type == TYPE_DV)
    {
        LOG_DEBUG("packet_type: TYPE_DV\n");
        assert(in_port > 1);

        LOG_DEBUG("test222 %d, %d\n", ((DV *)(packet + HEADER_LENGTH))->leaf_node_dis[0], ((DV *)(packet + HEADER_LENGTH))->leaf_node_dis[1]);
        dv_table[in_port] = *(DV *)(packet + HEADER_LENGTH);
        // memcpy(&dv_table[in_port], packet + HEADER_LENGTH, sizeof(DV));
        LOG_DEBUG("received from in_port=%d, dv from it:\n", in_port);
        dv_table[in_port].print_dv();

        DV former_local_dv = local_dv;
        update_local_dv();

        if (!(former_local_dv == local_dv))
        {
            LOG_DEBUG("former_local_dv != local_dv\n");
            make_packet((simulator_packet_t *)packet, 0, 0, TYPE_DV, sizeof(DV), (char *)&local_dv);
            ((DV *)(packet + HEADER_LENGTH))->leaf_node_dis[0] = 114514;
            LOG_DEBUG("test111 %d, %d\n", ((DV *)(packet + HEADER_LENGTH))->leaf_node_dis[0], ((DV *)(packet + HEADER_LENGTH))->leaf_node_dis[1]);
            LOG_DEBUG("exit router()\n\n");
            return 0; // 转发给所有邻居
        }
        else
        {
            LOG_DEBUG("former_local_dv == local_dv\n");
            LOG_DEBUG("exit router()\n\n");
            return -1; // 不转发
        }
    }
    else if (packet_type == TYPE_DATA)
    {
        LOG_DEBUG("packet_type: TYPE_DATA\n");
        for (int i = 0; i < MAX_LEAF_NODE_NUM; i++)
        {
            leaf_node_t leaf_node = local_dv.leaf_node[i];
            int leaf_node_dis = local_dv.leaf_node_dis[i];
            if (leaf_node.first == 0)
                continue;

            uint32_t l_ip = leaf_node.first, r_ip = leaf_node.first + (1u << leaf_node.second) - 1u; // 用于判断是否在叶子结点范围内
            if (l_ip <= dst && dst <= r_ip)                                                          // 如果目的地址在叶子结点范围内
            {
                int out_port = next_node_port_num[leaf_node];
                assert(leaf_node_dis < 0x3f3f3f3f);
                assert(out_port > 1);
                // 如果距离仍然不是0，说明还没有直连，需要转发给下一跳
                if (leaf_node_dis > 0)
                {
                    LOG_DEBUG("forward to another router\n");
                    assert(out_port != in_port);
                    assert(out_port > 1);
                    // assert(out_port != host_port);
                    assert(out_port != external_port);
                    LOG_DEBUG("exit router()\n\n");
                    return out_port;
                }
                else if (out_port == host_port[leaf_node])
                {
                    LOG_DEBUG("forward to host\n");
                    assert(host.find(leaf_node) != host.end());
                    LOG_DEBUG("exit router()\n\n");
                    return host_port[leaf_node];
                }
                else if (out_port == external_port)
                {
                    LOG_DEBUG("forward to external\n");
                    assert(leaf_node == external);
                    // 进行NAT
                    // 先判断是否已经分配了地址
                    if (nat_table_reverse[src])
                    {
                        assert(available_addr.find(nat_table_reverse[src]) == available_addr.end()); // 如果已经分配了地址，应当不在available_addr中
                        ((simulator_packet_t *)packet)->src = htonl(nat_table_reverse[src]);
                        src = nat_table_reverse[src]; // 包的src变了，局部变量src也要变
                        LOG_DEBUG("exit router()\n\n");
                        return external_port;
                    }
                    // 如果没有分配地址，就看看能不能分配一个地址
                    else
                    {
                        // 如果没有可用的地址，就丢弃这个包
                        if (available_addr.empty())
                        {
                            LOG_DEBUG("no available address\n");
                            LOG_DEBUG("exit router()\n\n");
                            return -1;
                        }
                        // 如果有可用的地址，就分配一个地址
                        else
                        {
                            uint32_t new_addr = *available_addr.begin();
                            available_addr.erase(new_addr);
                            nat_table_reverse[src] = new_addr;
                            nat_table[new_addr] = src;
                            ((simulator_packet_t *)packet)->src = htonl(new_addr);
                            src = new_addr; // 包的src变了，局部变量src也要变
                            LOG_DEBUG("exit router()\n\n");
                            return external_port;
                        }
                    }
                }
                else
                {
                    LOG_DEBUG("exit router()\n\n");
                    return 1;
                    LOG_FATAL("Invalid leaf_node: %08x/%d\n", leaf_node.first, leaf_node.second);
                }
            }
        }
    }
    else if (packet_type == TYPE_CONTROL)
    {
        LOG_DEBUG("packet_type: TYPE_CONTROL\n");

        char *command = (char *)(packet + HEADER_LENGTH);
        LOG_DEBUG("command: %s\n", command);

        if (*command == TRIGGER_DV_SEND)
        {
            LOG_DEBUG("command_type: TRIGGER_DV_SEND\n");
            make_packet((simulator_packet_t *)packet, 0, 0, TYPE_DV, sizeof(DV), (char *)&local_dv);
            LOG_DEBUG("exit router()\n\n");
            return 0; // 转发给所有邻居
        }
        else if (*command == RELEASE_NAT_ITEM)
        {
            LOG_DEBUG("command_type: RELEASE_NAT_ITEM\n");
            uint32_t addr = ntohl(inet_addr(command + 2));
            LOG_DEBUG("addr: %s\n", command + 2);
            available_addr.insert(addr);
            nat_table_reverse.erase(nat_table[addr]);
            nat_table.erase(addr);
        }
        else if (*command == PORT_VALUE_CHANGE)
        {
            LOG_DEBUG("command_type: PORT_VALUE_CHANGE\n");
            std::string tmp(command + 2);
            int port = std::stoi(tmp.substr(0, tmp.find(' ')));
            int value = std::stoi(tmp.substr(tmp.find(' ') + 1));
            LOG_DEBUG("port: %d, value: %d\n", port, value);

            DV former_local_dv = local_dv;

            // 如果value==-1，说明这个端口关闭了，需要把这个端口在变量中中删除
            if (value == -1)
            {
                LOG_DEBUG("close port: %d\n", port);
                // 根据next_node_port_num找哪些叶子结点的下一跳是这个端口，把 local_dv 中的这些叶子结点设置成不知道
                for (auto &i : next_node_port_num)
                {
                    if (i.second == port)
                    {
                        LOG_DEBUG("delete path to %s at port %d\n", leaf_node_to_string(i.first).c_str(), port);
                        // 把这个叶子结点从 local_dv 中删除
                        for (int j = 0; j < MAX_LEAF_NODE_NUM; ++j)
                        {
                            if (local_dv.leaf_node[j] == i.first)
                            {
                                local_dv.leaf_node[j] = std::make_pair(0, 0);
                                local_dv.leaf_node_dis[j] = 0x3f3f3f3f;
                            }
                        }
                        // 把这个叶子结点从 next_node_port_num 中删除
                        next_node_port_num.erase(i.first);
                    }
                }
                // 把这个端口从 dis 中删除
                dis.erase(port);
                // 把这个端口从 dv_table 中删除
                dv_table.erase(port);

                // 把这个端口从 host_port 和 host 中删除
                // 注意这里不需要释放公网地址，因为公网地址只在 RELEASE NAT ITEM 报文时释放
                for (auto &i : host_port)
                {
                    if (i.second == port)
                    {
                        host_port.erase(i.first);
                        host.erase(i.first);
                    }
                }

                // 如果这个端口是 external_port ，就把 external_port 设置成 0 ，并且把 external 删除
                // 注意这里不需要释放公网地址，因为公网地址已经没有意义了
                if (port == external_port)
                {
                    external_port = 0;
                    external = std::make_pair(0, 0);
                }
            }
            // 端口没有被关闭，只是改变了值
            else
            {
                dis[port] = value;
            }

            update_local_dv();

            if (!(former_local_dv == local_dv))
            {
                LOG_DEBUG("former_local_dv != local_dv\n");
                make_packet((simulator_packet_t *)packet, 0, 0, TYPE_DV, sizeof(DV), (char *)&local_dv);
                LOG_DEBUG("exit router()\n\n");
                return 0; // 转发给所有邻居
            }
            else
            {
                LOG_DEBUG("former_local_dv == local_dv\n");
                LOG_DEBUG("exit router()\n\n");
                return -1; // 不转发
            }
        }
        else if (*command == ADD_HOST)
        {
            LOG_DEBUG("command_type: ADD_HOST\n");
            std::string tmp(command + 2);
            int port = std::stoi(tmp.substr(0, tmp.find(' ')));
            tmp = tmp.substr(tmp.find(' ') + 1);
            uint32_t addr = ntohl(inet_addr(tmp.substr(0, tmp.find(' ')).c_str()));
            leaf_node_t leaf_node = std::make_pair(addr, 0);
            host.insert(leaf_node);
            host_port[leaf_node] = port;

            DV former_local_dv = local_dv;

            // 更新自己的距离向量
            local_dv.leaf_node[local_dv.new_leaf_node_pos] = leaf_node;
            local_dv.leaf_node_dis[local_dv.new_leaf_node_pos] = 0;
            local_dv.new_leaf_node_pos++;
            // 更新本地数据
            next_node_port_num[leaf_node] = port;
            dis[port] = 0;

            // 更新距离向量
            update_local_dv();

            if (!(former_local_dv == local_dv))
            {
                LOG_DEBUG("former_local_dv != local_dv\n");
                make_packet((simulator_packet_t *)packet, 0, 0, TYPE_DV, sizeof(DV), (char *)&local_dv);
                LOG_DEBUG("exit router()\n\n");
                return 0; // 转发给所有邻居
            }
            else
            {
                LOG_DEBUG("former_local_dv == local_dv\n");
                LOG_DEBUG("exit router()\n\n");
                return -1; // 不转发
            }
        }
        else if (*command == BLOCK_ADDR)
        {
            LOG_DEBUG("command_type: BLOCK_ADDR\n");
            uint32_t addr = ntohl(inet_addr(command + 2));
            block.insert(addr);
            LOG_DEBUG("exit router()\n\n");
            return -1;
        }
        else if (*command == UNBLOCK_ADDR)
        {
            LOG_DEBUG("command_type: UNBLOCK_ADDR\n");
            uint32_t addr = ntohl(inet_addr(command + 2));
            block.erase(addr);
            LOG_DEBUG("exit router()\n\n");
            return -1;
        }
        else
        {
            LOG_FATAL("Invalid command type: %s\n", command);
        }
    }
    else
    {
        LOG_FATAL("Invalid packet type: %d\n", packet_type);
    }
    LOG_DEBUG("exit router()\n\n");
    return 1;
}

void Router::update_local_dv()
{
    LOG_DEBUG("into update_local_dv()\n");
    LOG_DEBUG("    former_local_dv:\n");
    local_dv.print_dv();
    LOG_DEBUG("\n");

    // 用 new_nearest_dis 计算新的距离向量，key为叶子结点，value为最近的距离
    std::map<leaf_node_t, int> new_nearest_dis;
    // 先把所有已经有的叶子结点的距离加入到 new_nearest_dis 中
    for (int i = 0; i < MAX_LEAF_NODE_NUM; ++i)
    {
        if (local_dv.leaf_node[i].first == 0)
            continue;
        new_nearest_dis[local_dv.leaf_node[i]] = local_dv.leaf_node_dis[i];
    }
    // 然后把所有 dv_table 中的邻居距离叶子结点的距离，如果比已有的距离小，加入到 new_nearest_dis 中
    for (auto &i : dv_table)
    {
        int port = i.first;
        DV dv = i.second;
        LOG_DEBUG("    dv from port: %d\n", port);
        dv.print_dv();
        for (int j = 0; j < MAX_LEAF_NODE_NUM; ++j)
        {
            // 如果是空的叶子结点，就跳过
            if (dv.leaf_node[j].first == 0)
                continue;
            // 如果 new_nearest_dis 中没有这个叶子结点，就加入到 new_nearest_dis 中，并且更新 local_dv 和 next_node_port_num
            if (new_nearest_dis.find(dv.leaf_node[j]) == new_nearest_dis.end())
            {
                new_nearest_dis[dv.leaf_node[j]] = dv.leaf_node_dis[j] + dis[port];
                next_node_port_num[dv.leaf_node[j]] = port;
            }
            // 如果 new_nearest_dis 中有这个叶子结点，就比较距离，如果比已有的距离小，就更新 new_nearest_dis 中的距离，并且更新 local_dv 和 next_node_port_num
            else
            {
                if (new_nearest_dis[dv.leaf_node[j]] > dv.leaf_node_dis[j] + dis[port])
                {
                    new_nearest_dis[dv.leaf_node[j]] = dv.leaf_node_dis[j] + dis[port];
                    next_node_port_num[dv.leaf_node[j]] = port;
                }
            }
        }
    }

    int idx = 0;
    for (auto &i : new_nearest_dis)
    {
        local_dv.leaf_node[idx] = i.first;
        local_dv.leaf_node_dis[idx] = i.second;
        idx++;
    }
    // 更新 local_dv.new_leaf_node_pos ，找到第一个空的叶子结点，为下一次添加叶子结点做准备
    local_dv.new_leaf_node_pos = idx;
    LOG_DEBUG("\n");
    LOG_DEBUG("    latter_local_dv:\n");
    local_dv.print_dv();
    LOG_DEBUG("exit update_local_dv()\n");
    return;
}

DV Router::get_poison_reversed_dv(int port_num)
{
    LOG_DEBUG("into get_poison_reversed_dv()\n");

    DV poison_reversed_dv = local_dv;
    for (int i = 0; i < 512; ++i)
    {
        if (poison_reversed_dv.leaf_node[i].first == 0)
            continue;
        if (next_node_port_num[poison_reversed_dv.leaf_node[i]] == port_num)
            poison_reversed_dv.leaf_node_dis[i] = 0x3f3f3f3f;
    }

    local_dv.print_dv();
    poison_reversed_dv.print_dv();

    LOG_DEBUG("exit get_poison_reversed_dv()\n");
    return poison_reversed_dv;
}

void make_packet(simulator_packet_t *packet, uint32_t src, uint32_t dst, uint8_t type, uint16_t length, char *payload)
{
    LOG_DEBUG("into make_packet()\n");

    packet->src = htonl(src);
    packet->dst = htonl(dst);
    packet->type = type;
    packet->length = htons(length);
    memcpy(packet->payload, payload, length);
    // *(packet->payload + length) = '\0';

    packet->print();
    if (type == TYPE_DATA)
        LOG_DEBUG("    payload: %s\n", packet->payload);

    LOG_DEBUG("exit make_packet()\n");
    return;
}

std::string leaf_node_to_string(leaf_node_t leaf_node)
{
    struct in_addr inAddr;
    inAddr.s_addr = htonl(leaf_node.first);
    std::string ip = inet_ntoa(inAddr);
    std::string mask = std::to_string(leaf_node.second);
    return ip + "/" + mask;
}