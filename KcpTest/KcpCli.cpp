//
// Created by leo on 9/14/19.
//

#include "muduo/base/Logging.h"
#include "kcp/ikcp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

int g_udpSock = -1;

struct Option {
		std::string serverIp;
		uint16_t serverPort;
}g_option;

void init()
{
	g_udpSock = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
	assert(g_udpSock);

	sockaddr_in remote;
	remote.sin_family = AF_INET;
	remote.sin_addr.s_addr = inet_addr(g_option.serverIp.c_str());
	remote.sin_port = htons(g_option.serverPort);

	LOG_INFO << g_option.serverIp << ":" << g_option.serverPort;
	int ret = ::connect(g_udpSock, (const sockaddr*)&remote, sizeof(remote));
	if(ret != 0)
	{
		LOG_ERROR << "connect failed, errno: " << errno;
		assert(0);
	}
}

int output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	assert(g_udpSock >= 0);
	assert(len > 0);
	ssize_t bytes = -1;
	do
	{
		bytes = send(g_udpSock, buf, (size_t)len, 0);
	}
	while(bytes < 0 && errno == EAGAIN);

	//LOG_INFO << "c2s, bytes: " << bytes;

	return (int)bytes;
}

IUINT32 iclock()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return IUINT32(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void handle_input(ikcpcb* k)
{
	char rcvBuf[4096] = {0};
	sockaddr_in remote;
	socklen_t remoteLen = sizeof(remote);
	ssize_t bytes = recvfrom(g_udpSock, rcvBuf, sizeof(rcvBuf), 0, (sockaddr*)&remote, &remoteLen);
	if(bytes < 0)
	{
		if(errno != EAGAIN)
		{
			LOG_ERROR << "recvfrom failed, errno: " << errno;
		}
		return;
	}

	int inputRet = ikcp_input(k, rcvBuf, bytes);
	if(inputRet < 0)
	{
		LOG_WARN << "input failed, ret: " << inputRet;
		return;
	}

	int len = ikcp_peeksize(k);
	//assert(len == 0);
	if(len > 0)
	{
		LOG_INFO << "peek size: " << len;
	}
}

int main(int argc, char* argv[])
{
	if(argc != 3)
	{
		printf("Usage: %s <serverip> <serverport>\n", argv[0]);
		return -1;
	}

	g_option.serverIp = argv[1];
	g_option.serverPort = uint16_t(atoi(argv[2]));

	init();

	ikcpcb* k = ikcp_create(0, NULL);
	ikcp_nodelay(k, 1, 10, 2, 1);
	ikcp_setoutput(k, output);

	char buf[4096] = {0};
	memset(buf, 'a', sizeof(buf));

	double totalBytes = 0;
	IUINT32 logTs = iclock();

	IUINT32 ts = iclock();
	while(1)
	{
		IUINT32 now = iclock();
		if(now >= ts + 10)
		{
			ikcp_update(k, now);
			ts = now;
		}

		if(now >= logTs + 1000)
		{
			LOG_INFO << "send speed: " << totalBytes / 1024 << " KB/s";
			totalBytes = 0;
			logTs = now;
		}

		handle_input(k);

//		int waitSnd = ikcp_waitsnd(k);
//		if(waitSnd > 2048)
//		{
//			//LOG_WARN << "wait send: " << waitSnd;
//			continue;
//		}

		int ret = ikcp_send(k, buf, sizeof(buf));
		if(ret < 0)
		{
			LOG_ERROR << "send failed, ret: %d" << ret;
			break;
		}
		totalBytes += sizeof(buf);
		usleep(1000);
	}

	ikcp_release(k);

	return 0;
}