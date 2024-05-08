class RouterBase
{
public:
    virtual void router_init(int port_num, int external_port, char *external_addr, char *available_addr) = 0;

    /* return out_port, 0 = broadcast, -1 = drop, 1 = default */
    virtual int router(int in_port, char *packet) = 0;
};

RouterBase *create_router_object();