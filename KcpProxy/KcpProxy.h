//
// Created by leo on 2/4/19.
//

#ifndef PROXY_KCPPROXY_H
#define PROXY_KCPPROXY_H

#include <muduo/net/EventLoop.h>
#include "muduo/net/TcpServer.h"
#include "muduo/net/Channel.h"
#include "KcpSession.h"
#include <map>
#include <memory>

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


private:
	EventLoop loop_;
	TcpServer server_;
	Channel kcpChan_;

	typedef std::shared_ptr<KcpSession> KcpSessionPtr;
	typedef std::map<IUINT32, KcpSessionPtr> KcpSessionMap;

	KcpSessionMap sessionMap_;
};


#endif //PROXY_KCPPROXY_H
