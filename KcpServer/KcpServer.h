//
// Created by leo on 2/5/19.
//

#ifndef PROXY_KCPSERVER_H
#define PROXY_KCPSERVER_H

#include <muduo/net/Channel.h>
#include "muduo/net/EventLoop.h"
#include "KcpServer/KcpServerSession.h"
#include <set>

using namespace muduo;
using namespace muduo::net;

typedef std::set<IUINT32> DeadSessionSet;

class KcpServer {
public:
	KcpServer();
	~KcpServer();

public:
	void run();

private:
	int  initUdpTunnel();
	void onUdpTunnelReadable(Timestamp);
	void onUpdateKcpTimer();
	void onCheckDeadSessionTimer();
	void onGetDeadSessionNotity();

	void dispatchKcpPacket(const char* packet, size_t len, sockaddr_in* remote);
	void notifyPeerException(uint32_t sessId, sockaddr_in* remote);

private:
	EventLoop loop_;
	int udpServer_;
	Channel udpServerChan_;
	KcpServerSessionMap sessionMap_;
    DeadSessionSet deadSessionSet_;
};




#endif //PROXY_KCPSERVER_H
