//
// Created by leo on 2/5/19.
//

#ifndef PROXY_KCPSERVER_H
#define PROXY_KCPSERVER_H

#include <muduo/net/Channel.h>
#include "muduo/net/EventLoop.h"
#include "KcpServer/KcpServerSession.h"

using namespace muduo;
using namespace muduo::net;

class KcpServer {
public:
	KcpServer();
	~KcpServer();

public:
	void run();

private:
	int initUdpTunnel();
	void onUdpTunnelReadable(Timestamp);
	void onUpdateKcpTimer();
	void onCheckDeadSessionTimer();

private:
	EventLoop loop_;
	int udpServer_;
	Channel udpServerChan_;
	KcpServerSessionMap sessionMap_;
};




#endif //PROXY_KCPSERVER_H
