//
// Created by leo on 2/6/19.
//

#ifndef PROXY_KCPSERVERSESSION_H
#define PROXY_KCPSERVERSESSION_H

#include "muduo/net/TcpClient.h"
#include "muduo/base/Timestamp.h"
#include "kcp/ikcp.h"
#include <map>
#include <memory>
#include <list>
#include <functional>
#include "muduo/net/TimerId.h"
#include "muduo/net/EventLoop.h"

using namespace muduo;
using namespace muduo::net;

class KcpServerSession
{
public:
	KcpServerSession(EventLoop* loop, IUINT32 id, sockaddr_in* remote);
	~KcpServerSession();

	void connectSvr();
	void disconnectFromSvr();

	void recvFromTunnel(const char* packet, size_t len);
	sockaddr_in* getTunnelPeer();
	void updateKcp();
	bool isDead();
    string getIdStr();

    static int output(const char *buf, int len, ikcpcb *kcp, void *user);

private:
	void extractTunnelPayload(const std::string &input, std::string &output);
	void onConnection(const TcpConnectionPtr& conn);
	void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp);
	void onCheckConnection();

private:
    muduo::net::EventLoop* loop_;
	ikcpcb* kcp_;
	std::shared_ptr<TcpClient> client_;
	bool connecting_;
	sockaddr_in remoteAddr_;
	std::list<std::string> payloadList_;
	Timestamp lastTunnelActiveTime_;
};

typedef std::shared_ptr<KcpServerSession> KcpServerSessionPtr;
typedef std::map<IUINT32, KcpServerSessionPtr> KcpServerSessionMap;


#endif //PROXY_KCPSERVERSESSION_H
