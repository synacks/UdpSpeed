//
// Created by leo on 2/5/19.
//

#ifndef PROXY_KCPSESSION_H
#define PROXY_KCPSESSION_H

#include "kcp/ikcp.h"
#include <string>
#include <memory>
#include <muduo/net/TcpConnection.h>


typedef std::weak_ptr<muduo::net::TcpConnection> TcpConnectionRef;

class KcpSession
{
public:
	KcpSession(TcpConnectionRef ref);
	~KcpSession();

	void sendToTunnel(const void *data, size_t len);
	bool recvFromTunnel(const std::string &input, std::string &output);

	void sendToConn(const std::string& data);

	void update();
	IUINT32 id() {return id_;}

private:
	ikcpcb* kcp_;
	IUINT32 id_;
	TcpConnectionRef conn_;
};

typedef std::weak_ptr<KcpSession> KcpSessionRef;

#endif //PROXY_KCPSESSION_H
