//
// Created by leo on 2/6/19.
//

#include <muduo/base/Logging.h>
#include "KcpServerSession.h"
#include <errno.h>
#include "Option.h"
#include <memory>
#include <sstream>

extern int g_udpServer;

using namespace std;



int KcpServerSession::output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	KcpServerSession* sess = (KcpServerSession*)user;
	assert(sess);

	assert(g_udpServer >= 0);
	assert(len > 0);
	ssize_t bytes = 0;

	do
	{
		bytes = sendto(g_udpServer, buf, (size_t)len, 0,
					   (const sockaddr*)sess->getTunnelPeer(), sizeof(sockaddr));
	}
	while(bytes < 0 && errno == EAGAIN);

	assert(bytes == len);
	LOG_INFO << sess->getIdStr() << " C -> B, bytes: " << bytes;
	return (int)bytes;
}

KcpServerSession::KcpServerSession(EventLoop* loop, IUINT32 id, sockaddr_in* remote)
	: loop_(loop)
	, client_(make_shared<TcpClient>(loop, InetAddress(g_option.destIp, g_option.destPort), "tcpclient"))
	, connecting_(true)
{
	kcp_ = ikcp_create(id, this);
	ikcp_nodelay(kcp_, 1, 10, 1, 1);
	ikcp_setoutput(kcp_, output);
    lastTunnelActiveTime_ = Timestamp::now();
    memcpy(&remoteAddr_, remote, sizeof(remoteAddr_));
}

KcpServerSession::~KcpServerSession()
{
	ikcp_flush(kcp_);
	ikcp_release(kcp_);
	LOG_INFO << getIdStr() << " server session destroyed!";
}

void KcpServerSession::connectSvr()
{
	client_->setConnectionCallback(std::bind(&KcpServerSession::onConnection, this, _1));
	client_->setMessageCallback(std::bind(&KcpServerSession::onMessage, this, _1, _2, _3));
	client_->connect();
	loop_->runAfter(3, std::bind(&KcpServerSession::onCheckConnection, this));
}

void KcpServerSession::disconnectFromSvr()
{
    client_->disconnect();
}

void KcpServerSession::recvFromTunnel(const char* packet, size_t len)
{
	IUINT32 sn = ikcp_getsn(packet, len);
	LOG_INFO << getIdStr() << " B -> C, bytes: " << len << ", sn: " << sn;

	std::string payload;
	extractTunnelPayload(std::string(packet, (size_t)len), payload);
	if(payload.empty())
	{
		LOG_DEBUG << "extract payload from tunnel packet failed";
		return;
	}

    lastTunnelActiveTime_ = Timestamp::now();

    TcpConnectionPtr conn = client_->connection();
	if(conn)
	{
		while(!payloadList_.empty())
		{
			conn->send(payloadList_.front());
			payloadList_.pop_front();
		}
		conn->send(payload);
	}
	else
	{
		payloadList_.push_back(payload);
	}
}

void KcpServerSession::extractTunnelPayload(const std::string &input, std::string &output)
{
	int inputRet = ikcp_input(kcp_, input.c_str(), input.length());
	if(inputRet < 0)
	{
		LOG_WARN << "get invalid kcp packet, ret: " << inputRet;
		return;
	}

	int peekSize = ikcp_peeksize(kcp_);
	if(peekSize <= 0)
	{
	    if(input.length() != 24)
        {
            LOG_ERROR << "extract tunnel payload failed!";
        }
		return;
	}

	output.resize((size_t)peekSize, '\0');
	int recvRet = ikcp_recv(kcp_, (char*)output.data(), peekSize);
	if(recvRet <= 0)
	{
		LOG_WARN << "recv failed, ret: " << recvRet;
		return;
	}

	output.resize((size_t)recvRet);
	LOG_INFO << getIdStr() << " C -> D, bytes: " << recvRet;
}

void KcpServerSession::onConnection(const TcpConnectionPtr& conn)
{
    connecting_ = false;
	if(conn->connected())
	{
	    LOG_TRACE << "connection established!";

		while(!payloadList_.empty())
		{
			conn->send(payloadList_.front());
			LOG_TRACE << "found data cached while connecting dest server"
							 << ", data: " << &payloadList_.front()
							 << ", length: " << payloadList_.front().length();
			payloadList_.pop_front();
		}
	}
	else
	{
		LOG_INFO << "server session disconnected!";
	}
}

void KcpServerSession::onCheckConnection()
{
    if(!client_->connection())
    {
        client_->stop();
        connecting_ = false;
    }
}

void KcpServerSession::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
{
	int sendRet = ikcp_send(kcp_, buf->peek(), (int)buf->readableBytes());
	assert(sendRet == 0);
	LOG_INFO << getIdStr() << " D -> C, bytes: " << buf->readableBytes();
	buf->retrieveAll();
}

sockaddr_in* KcpServerSession::getTunnelPeer()
{
	return &remoteAddr_;
}

static IUINT32 iclock()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return IUINT32(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void KcpServerSession::updateKcp()
{
	ikcp_update(kcp_, iclock());
}

bool KcpServerSession::isDead()
{
    if(connecting_)
    {
        return false;
    }

	if(!client_->connection())
	{
		return true;
	}

	//与代理端长时间没有交互
	double delta = timeDifference(Timestamp::now(), lastTunnelActiveTime_);
	return delta > 1.0 * 3600;
}

string KcpServerSession::getIdStr()
{
    ostringstream oss;
    oss << "[" << this << " " << kcp_->conv << "]";
    return oss.str();
}