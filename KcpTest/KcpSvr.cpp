//
// Created by leo on 9/14/19.
//
#include "muduo/base/Logging.h"
#include "kcp/ikcp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;


struct Option
{
		uint16_t localPort;
}g_option;

int g_udpServer = -1;
sockaddr_in g_remote = {0};

int initUdpTunnel()
{
	int fd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
	assert(fd >= 0);
	sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = inet_addr("0.0.0.0");
	local.sin_port = htons(g_option.localPort);
	assert(0 == bind(fd, (sockaddr*)&local, sizeof(local)));

	g_udpServer = fd;
	return fd;
}

IUINT32 iclock()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return IUINT32(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}


int output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	assert(g_udpServer >= 0);
	assert(len > 0);
	ssize_t bytes = -1;
	do
	{
		bytes = sendto(g_udpServer, buf, (size_t)len, 0, (sockaddr*)&g_remote, sizeof(sockaddr_in));
	}
	while(bytes < 0 && errno == EAGAIN);

	//LOG_INFO << "c2s, bytes: " << bytes;

	return (int)bytes;
}



int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		printf("usage: %s <localPort>\n", argv[0]);
		return -1;
	}

	g_option.localPort = (uint16_t)atoi(argv[1]);

	initUdpTunnel();
	ikcpcb* kcp_ = ikcp_create(0, NULL);
	ikcp_nodelay(kcp_, 1, 10, 2, 1);
	ikcp_setoutput(kcp_, output);
	kcp_->rcv_wnd = 1024;


	IUINT32 ts = iclock();
	IUINT32 ts0 = ts;
	IUINT32 tsUpdate = ts;

	double totalRcvBytes = 0;
	double totalBytes = 0;
	while(1)
	{
		IUINT32 now = iclock();
		if(now >= tsUpdate + 10)
		{
			ikcp_update(kcp_, now);
			tsUpdate = now;
		}

		char buf[4096] = {0};
		socklen_t remoteLen = sizeof(g_remote);
		ssize_t bytes = recvfrom(g_udpServer, buf, sizeof(buf), 0, (sockaddr*)&g_remote, &remoteLen);
		if(bytes <= 0)
		{
			if(errno != EAGAIN)
			{
				LOG_ERROR << "recv failed, errno: %d" << errno;
			}
			continue;
		}

		totalRcvBytes += bytes;
		int inputRet = ikcp_input(kcp_, buf, bytes);
		if(inputRet < 0)
		{
			LOG_WARN << "input failed, ret: " << inputRet;
		}

		while(1)
		{
			int len = ikcp_peeksize(kcp_);
			if(len <= 0)
			{
				break;
			}
			string output;
			output.resize((size_t)len, '\0');
			int ret = ikcp_recv(kcp_, (char*)output.c_str(), len);
			assert(ret == len);
			totalBytes += ret;

			len = ikcp_peeksize(kcp_);
		}

		now = iclock();
		if(now > ts + 1000)
		{
			LOG_INFO << "kcp     speed: " << totalRcvBytes / 1024 / (now - ts0) * 1000 << " KB/s";
			LOG_INFO << "payload speed: " << totalBytes / 1024 / (now - ts0) * 1000 << " KB/s";
			ts = now;
		}
	}


	return 0;
}