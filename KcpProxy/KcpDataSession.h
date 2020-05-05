//
// Created by leo on 2/5/19.
//

#ifndef PROXY_KCPDATASESSION_H
#define PROXY_KCPDATASESSION_H

#include "kcp/ikcp.h"
#include <string>
#include <memory>
#include <muduo/net/TcpConnection.h>
#include <list>


typedef std::weak_ptr<muduo::net::TcpConnection> TcpConnectionRef;

class KcpDataSession
{
public:
	explicit KcpDataSession(TcpConnectionRef ref);
	~KcpDataSession();

	void init(uint32_t id);

	void sendToTunnel(const void *data, size_t len);
	int recvFromTunnel(const std::string &input);

	void update();
	IUINT32 id() {return id_;}

private:
	ikcpcb* kcp_ = nullptr;
	IUINT32 id_ = 0;
	TcpConnectionRef conn_;
	std::list<std::string> payloadList_;
};

typedef std::shared_ptr<KcpDataSession> KcpDataSessionPtr;
typedef std::weak_ptr<KcpDataSession> KcpDataSessionRef;


#endif //PROXY_KCPDATASESSION_H
