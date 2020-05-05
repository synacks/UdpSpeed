//
// Created by leo on 2/5/19.
//

#include "KcpTunnel.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <muduo/base/Logging.h>
#include <KcpProxy/Option.h>
#include "Option.h"
#include "KcpDataSession.h"


int KcpTunnel::g_udpSock = -1;

void KcpTunnel::init()
{
	g_udpSock = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
	assert(g_udpSock);

	sockaddr_in remote;
	remote.sin_family = AF_INET;
	remote.sin_addr.s_addr = inet_addr(g_option.serverIp.c_str());
	remote.sin_port = htons(g_option.serverPort);

	LOG_INFO << "started with remote ip: " << g_option.serverIp  << ", port: " << g_option.serverPort;
	int ret = connect(g_udpSock, (const sockaddr*)&remote, sizeof(remote));
	if(ret != 0)
	{
		LOG_ERROR << "connect failed, errno: " << errno;
		assert(0);
	}
}

int KcpTunnel::output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	assert(g_udpSock >= 0);
	assert(len > 0);
	ssize_t bytes = -1;
	do
	{
		bytes = send(g_udpSock, buf, (size_t)len, 0);
	}
	while(bytes < 0 && errno == EAGAIN);

	KcpDataSession* sess = (KcpDataSession*)user;
	if(sess)
    {
        LOG_INFO << "["<< sess->id() << "] B -> C, bytes: " << bytes;
    }

	return (int)bytes;
}