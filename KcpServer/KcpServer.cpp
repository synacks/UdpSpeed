//
// Created by leo on 2/5/19.
//

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <muduo/base/Logging.h>
#include "KcpServer.h"

using namespace std;


KcpServer::KcpServer()
	: udpServer_(initUdpTunnel())
	, udpServerChan_(&loop_, udpServer_)
{

}

KcpServer::~KcpServer()
{

}

void KcpServer::run()
{
	udpServerChan_.setReadCallback(std::bind(&KcpServer::onUdpTunnelReadable, this, _1));
	udpServerChan_.enableReading();
	loop_.runEvery(0.01, std::bind(&KcpServer::onUpdateKcpTimer, this));
	loop_.runEvery(15, std::bind(&KcpServer::onCheckDeadSessionTimer, this));
	loop_.loop();
}

int g_udpServer = -1;

int KcpServer::initUdpTunnel()
{
	int fd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
	assert(fd >= 0);
	sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = inet_addr("0.0.0.0");
	local.sin_port = htons(6667);
	assert(0 == bind(fd, (sockaddr*)&local, sizeof(local)));

	g_udpServer = fd;
	return fd;
}

void KcpServer::onDeadSessionNotity()
{
	char buf[32] = {0};
	sockaddr_in remote;
	socklen_t remoteLen = sizeof(remote);
	ssize_t bytes = recvfrom(udpServer_, buf, sizeof(buf), 0, (sockaddr*)&remote, &remoteLen);
	if(bytes == 8)
	{
		uint32_t deadSessId = ntohl(*(uint32_t*)(buf + 4));
		KcpServerSessionMap::iterator it = sessionMap_.find(deadSessId);
		if(it != sessionMap_.end())
		{
			const sockaddr_in* peer = it->second->getTunnelPeer();
			if(0 == memcmp(&peer->sin_addr, &remote.sin_addr, sizeof(remote.sin_addr)) &&
					peer->sin_port == remote.sin_port)
			{
        LOG_INFO << "erase dead session: " << deadSessId;
        it->second->disconnectFromSvr();
				sessionMap_.erase(it);
			}
		}
	}
}

void KcpServer::onUdpTunnelReadable(Timestamp)
{
	char buf[4] = {0};
	sockaddr_in remote;
	socklen_t remoteLen = sizeof(remote);
	ssize_t bytes = recvfrom(udpServer_, buf, sizeof(buf), MSG_PEEK, (sockaddr*)&remote, &remoteLen);
	if(bytes < 0)
	{
		LOG_ERROR << "recv from udpserver failed, errno: " << errno;
		return;
	}

	IUINT32 id = ikcp_getconv(buf);
	if(id == 0)
	{
		onDeadSessionNotity();
		return;
	}

	LOG_INFO << "recv packet from " << id;

	auto it = sessionMap_.find(id);
	if(it == sessionMap_.end())
	{
		KcpServerSessionPtr sess = make_shared<KcpServerSession>(&loop_, id);
		sess->connectSvr();
		sessionMap_[id] = sess;
	}

	sessionMap_[id]->recvFromTunnel();
}

void KcpServer::onUpdateKcpTimer()
{
	for(auto it = sessionMap_.begin(); it != sessionMap_.end(); ++it)
	{
		it->second->updateKcp();
	}
}

void KcpServer::onCheckDeadSessionTimer()
{
	auto it = sessionMap_.begin();
	while(it != sessionMap_.end())
	{
		auto cur = it++;
		if(cur->second->isDead())
		{
			cur->second->disconnectFromSvr();
			sessionMap_.erase(cur);
		}
	}
}