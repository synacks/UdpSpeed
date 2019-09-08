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

using namespace muduo;
using namespace muduo::net;

class KcpServerSession : public std::enable_shared_from_this<KcpServerSession>
{
public:
	KcpServerSession(EventLoop* loop, IUINT32 id);
	~KcpServerSession();

	void connectSvr();
	void disconnectFromSvr();

	void recvFromTunnel();
	sockaddr_in* getTunnelPeer();
	void updateKcp();
	bool isDead();

private:
	void extractTunnelPayload(const std::string &input, std::string &output);
	void onConnection(const TcpConnectionPtr& conn);
	void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp);

private:
	ikcpcb* kcp_;
	std::shared_ptr<TcpClient> client_;
	sockaddr_in remoteAddr_;
	TcpConnectionPtr conn_;
	std::list<std::string> payloadList_;
	Timestamp lastActiveTime_;
	bool disconnected_;
};

typedef std::shared_ptr<KcpServerSession> KcpServerSessionPtr;
typedef std::map<IUINT32, KcpServerSessionPtr> KcpServerSessionMap;


#endif //PROXY_KCPSERVERSESSION_H
