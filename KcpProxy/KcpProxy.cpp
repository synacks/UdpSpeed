//
// Created by leo on 2/4/19.
//

#include "KcpProxy.h"
#include "muduo/net/InetAddress.h"
#include "muduo/base/Logging.h"
#include <functional>
#include "kcp/ikcp.h"
#include "KcpTunnel.h"

using namespace std;



KcpProxy::KcpProxy()
		: server_(&loop_, InetAddress(6666), "KcpProxy", TcpServer::kReusePort)
		, kcpChan_(&loop_, KcpTunnel::g_udpSock)
{}

void KcpProxy::run()
{
	kcpChan_.setReadCallback(std::bind(&KcpProxy::onKcpTunnelMessage, this, _1));
	kcpChan_.enableReading();

	server_.setThreadNum(0);
	server_.setConnectionCallback(std::bind(&KcpProxy::onConnection, this, placeholders::_1));
	server_.setMessageCallback(std::bind(&KcpProxy::onMessage, this, _1, _2, _3));
	server_.start();
	loop_.runEvery(0.01, std::bind(&KcpProxy::onUpdateTunnelTimer, this));
	loop_.loop();
}

void KcpProxy::onConnection(const TcpConnectionPtr& conn)
{
	LOG_INFO << "connection " << (conn->connected() ? "established " : "disconnected ")
			 << conn->peerAddress().toIpPort()
			 << " -> "
			 << conn->localAddress().toIpPort();
	if(conn->connected())
	{
		KcpSessionPtr sess = make_shared<KcpSession>(conn);
		sessionMap_[sess->id()] = sess;
		KcpSessionRef sessRef(sess);
		conn->setContext(sessRef);
	}
	else
	{
		KcpSessionRef sessRef = boost::any_cast<KcpSessionRef>(conn->getContext());
		KcpSessionPtr sess = sessRef.lock();
		assert(sess);
		//TODO: send connection close message to server
		sessionMap_.erase(sess->id());
	}

}

void KcpProxy::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
{
	KcpSessionRef sessRef = boost::any_cast<KcpSessionRef>(conn->getContext());
	KcpSessionPtr sess = sessRef.lock();
	assert(sess);
	sess->sendToTunnel(buf->peek(), buf->readableBytes());
	LOG_INFO << "A -> B, bytes: " << buf->readableBytes();
	buf->retrieveAll();
}


void KcpProxy::onKcpTunnelMessage(Timestamp)
{
	char buf[4096] = {0};
	ssize_t ret = read(KcpTunnel::g_udpSock, buf, sizeof(buf));
	if(ret < 0)
	{
		LOG_ERROR << "read error, errno: " << errno;
		return;
	}

	IUINT32 sessId = ikcp_getconv(buf);
	KcpSessionMap::iterator it = sessionMap_.find(sessId);
	if(sessionMap_.end() == it)
	{
		LOG_WARN << "invalid session id: " << sessId;
		return;
	}

	LOG_INFO << "C -> B, bytes: " << ret;

	KcpSessionPtr sess = it->second;
	std::string payload;
	if(!sess->recvFromTunnel(std::string(buf, (size_t) ret), payload))
	{
		LOG_WARN << "recv from tunnel failed";
		return;
	}

	sess->sendToConn(payload);
	LOG_INFO << "B -> A, bytes: " << payload.length();
}

void KcpProxy::onUpdateTunnelTimer()
{
	for(auto it = sessionMap_.begin(); it != sessionMap_.end(); ++it)
	{
		it->second->update();
	}
}
