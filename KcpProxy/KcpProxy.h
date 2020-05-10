//
// Created by leo on 2/4/19.
//

#ifndef PROXY_KCPPROXY_H
#define PROXY_KCPPROXY_H

#include <muduo/net/EventLoop.h>
#include "muduo/net/TcpServer.h"
#include "muduo/net/Channel.h"
#include "KcpDataSession.h"
#include "kcp/ControlSession.h"
#include <map>
#include <memory>
#include <queue>

using namespace muduo;
using namespace muduo::net;

class KcpProxy
{
public:
	KcpProxy();

	void run();

private:
	void onConnection(const TcpConnectionPtr& conn);
	void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp);
	void onKcpTunnelMessage(Timestamp);
	void onUpdateTunnelTimer();
	void handleControlMessage(const std::string& controlMsg);
	void notifyDeadSessionId(uint32_t id);

private:
	EventLoop loop_;
	TcpServer server_;
	Channel kcpChan_;
	ControlSession controlSess_;
	std::map<IUINT32, KcpDataSessionRef> sessionMap_;
	std::list<KcpDataSessionRef> uninitedSessList_;
};


#endif //PROXY_KCPPROXY_H
