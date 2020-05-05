//
// Created by leo on 2/5/19.
//

#ifndef PROXY_KCPPROXYSESSION_H
#define PROXY_KCPPROXYSESSION_H

#include "kcp/ikcp.h"
#include <string>
#include <memory>
#include <muduo/net/TcpConnection.h>


typedef std::weak_ptr<muduo::net::TcpConnection> TcpConnectionRef;

class KcpProxySession
{
public:
	explicit KcpProxySession(TcpConnectionRef ref);
	~KcpProxySession();

	void sendToTunnel(const void *data, size_t len);
	int recvFromTunnel(const std::string &input);

	void update();
	IUINT32 id() {return id_;}

private:
	ikcpcb* kcp_;
	IUINT32 id_;
	TcpConnectionRef conn_;
};

typedef std::weak_ptr<KcpProxySession> KcpSessionRef;

#endif //PROXY_KCPPROXYSESSION_H
