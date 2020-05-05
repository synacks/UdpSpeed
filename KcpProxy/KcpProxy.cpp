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
		: server_(&loop_, InetAddress(6665), "KcpProxy", TcpServer::kReusePort)
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
		KcpSessionPtr sess = make_shared<KcpProxySession>(conn);
		sessionMap_[sess->id()] = sess;
		KcpSessionRef sessRef(sess);
		conn->setContext(sessRef);
	}
	else
	{
		KcpSessionRef sessRef = boost::any_cast<KcpSessionRef>(conn->getContext());
		KcpSessionPtr sess = sessRef.lock();
		if(sess)
		{
            sessionMap_.erase(sess->id());
        }
		else
        {
		    LOG_INFO << "session disappeared before connection!";
		}
	}
}

void KcpProxy::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
{
	KcpSessionRef sessRef = boost::any_cast<KcpSessionRef>(conn->getContext());
	KcpSessionPtr sess = sessRef.lock();
	if(sess)
    {
        sess->sendToTunnel(buf->peek(), buf->readableBytes());
        buf->retrieveAll();
    }
	else
    {
	    LOG_INFO << "session does not exist any more, close it!";
	    conn->forceClose();
    }
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

	//sessId为0表示为管理会话
	if(sessId == 0)
	{
		if(ret == 8)
		{
			IUINT32 deadSessId = ntohl(*(uint32_t*)(buf + 4));
			LOG_WARN << "found dead session id: " << deadSessId;
			KcpSessionMap::iterator it = sessionMap_.find(deadSessId);
			if(it != sessionMap_.end())
			{
			    //TODO:关闭session对应的TCP连接
				sessionMap_.erase(it);
				LOG_WARN << "remove the dead session，id: " << deadSessId;
			}
		}
		return;
	}

	KcpSessionMap::iterator it = sessionMap_.find(sessId);
	if(sessionMap_.end() == it)
	{
		LOG_WARN << "get invalid session id: " << sessId;
		uint32_t packet[2] = {0, htonl(sessId)};
		KcpTunnel::output((char*)packet, sizeof(packet), NULL, this);
		return;
	}

	LOG_INFO << "["<< sessId << "] C -> B, bytes: " << ret;

	KcpSessionPtr sess = it->second;
	if(sess->recvFromTunnel(std::string(buf, (size_t)ret)) < 0)
	{
		LOG_WARN << "recv from tunnel failed";
		return;
	}
}

void KcpProxy::onUpdateTunnelTimer()
{
	for(auto it = sessionMap_.begin(); it != sessionMap_.end(); ++it)
	{
		it->second->update();
	}
}
