//
// Created by leo on 2/5/19.
//

#include <cstdint>
#include <muduo/base/Logging.h>
#include "KcpSession.h"
#include "KcpTunnel.h"


using namespace std;
using namespace muduo;
using namespace muduo::net;

static IUINT32 s_sessId = 0;

IUINT32 iclock()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return IUINT32(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

KcpSession::KcpSession(TcpConnectionRef ref)
	: conn_(ref)
{
	id_ = s_sessId++;
	kcp_ = ikcp_create(id_, nullptr);
	ikcp_setoutput(kcp_, &KcpTunnel::output);
	ikcp_nodelay(kcp_, 1, 10, 1, 1);
}

KcpSession::~KcpSession()
{
	ikcp_flush(kcp_);
	ikcp_release(kcp_);
}

void KcpSession::update()
{
	ikcp_update(kcp_, iclock());
}

void KcpSession::sendToTunnel(const void *data, size_t len)
{
	int ret = ikcp_send(kcp_, (const char*)data, (int)len);
	if(ret != 0)
	{
		LOG_ERROR << "ikcp_send failed, ret: " << ret;
	}
}

bool KcpSession::recvFromTunnel(const std::string &input, string &output)
{
	int inputRet = ikcp_input(kcp_, input.data(), (long)input.length());
	if(inputRet < 0)
	{
		LOG_WARN << "input failed, ret: " << inputRet;
		return false;
	}

	int len = ikcp_peeksize(kcp_);
	if(len <= 0)
	{
		LOG_WARN << "peek found no output";
		output.clear();
		return false;
	}

	output.resize((size_t)len, '\0');
	int ret = ikcp_recv(kcp_, (char*)output.c_str(), len);
	assert(ret >= 0);
	output.resize((size_t)ret);
	return true;
}

void KcpSession::sendToConn(const std::string& data)
{
	TcpConnectionPtr connPtr = conn_.lock();
	assert(connPtr);
	connPtr->send(data);
}
