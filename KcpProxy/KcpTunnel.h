//
// Created by leo on 2/5/19.
//

#ifndef PROXY_KCPTUNNEL_H
#define PROXY_KCPTUNNEL_H

#include <kcp/ikcp.h>

class KcpTunnel {
public:
	static void init();
	static int output(const char *buf, int len, ikcpcb *kcp, void *user);
	static int g_udpSock;
};


#endif //PROXY_KCPTUNNEL_H
