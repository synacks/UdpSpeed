//
// Created by leo on 2/6/19.
//

#include <muduo/base/Logging.h>
#include "KcpServerSession.h"
#include <errno.h>
#include "Option.h"
#include <memory>

extern int g_udpServer;

using namespace std;



int output(const char *buf, int len, ikcpcb *kcp, void *user)
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
	LOG_INFO << "C -> B, bytes: " << bytes;
	return (int)bytes;
}

KcpServerSession::KcpServerSession(EventLoop* loop, IUINT32 id)
	: client_(make_shared<TcpClient>(loop, InetAddress(g_option.destIp, g_option.destPort), "tcpclient"))
	, disconnected_(false)
{
	kcp_ = ikcp_create(id, this);
	ikcp_nodelay(kcp_, 1, 10, 1, 1);
	ikcp_setoutput(kcp_, output);
	lastActiveTime_ = Timestamp::now();
}

KcpServerSession::~KcpServerSession()
{
	ikcp_flush(kcp_);
	ikcp_release(kcp_);
}

void KcpServerSession::connectSvr()
{
	client_->setConnectionCallback(std::bind(&KcpServerSession::onConnection, shared_from_this(), _1));
	client_->setMessageCallback(std::bind(&KcpServerSession::onMessage, shared_from_this(), _1, _2, _3));
	client_->connect();
}

void KcpServerSession::disconnectFromSvr()
{
	client_->setConnectionCallback(defaultConnectionCallback);
	client_->setMessageCallback(defaultMessageCallback);
	client_->disconnect();
	disconnected_ = true;
}

void KcpServerSession::recvFromTunnel()
{
	char buf[4096] = {0};

	socklen_t remoteLen = sizeof(remoteAddr_);
	ssize_t ret = recvfrom(g_udpServer, buf, sizeof(buf), 0, (sockaddr*)&remoteAddr_, &remoteLen);
	if(ret < 0)
	{
		LOG_ERROR << "recv failed, errno: " << errno;
		return;
	}

	LOG_INFO << "B -> C, bytes: " << ret;

	std::string payload;
	extractTunnelPayload(std::string(buf, (size_t)ret), payload);
	if(payload.empty())
	{
		LOG_DEBUG << "extract payload from tunnel packet failed";
		return;
	}

	lastActiveTime_ = Timestamp::now();

	if(conn_)
	{
		while(!payloadList_.empty())
		{
			conn_->send(payloadList_.front());
			payloadList_.pop_front();
		}
		conn_->send(payload);
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
		LOG_WARN << "recv found no data";
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
	LOG_INFO << "C -> D, bytes: " << recvRet;
}

void KcpServerSession::onConnection(const TcpConnectionPtr& conn)
{
	if(conn->connected())
	{
		conn_ = conn;
		while(!payloadList_.empty())
		{
			conn_->send(payloadList_.front());
			LOG_INFO << "found data cached while connecting dest server"
							 << ", data: " << &payloadList_.front()
							 << ", length: " << payloadList_.front().length();
			payloadList_.pop_front();
		}
	}
	else
	{
		conn_.reset();
		disconnected_ = true;
	}
}


void KcpServerSession::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
{
	int sendRet = ikcp_send(kcp_, buf->peek(), (int)buf->readableBytes());
	assert(sendRet == 0);
	LOG_INFO << "D -> C, bytes: " << buf->readableBytes();
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
	if(disconnected_)
	{
		return true;
	}
	double delta = timeDifference(Timestamp::now(), lastActiveTime_);
	return delta > 2.0 * 15;
}