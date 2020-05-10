//
// Created by leo on 2/5/19.
//

#include <cstdint>
#include <muduo/base/Logging.h>
#include "KcpDataSession.h"
#include "KcpTunnel.h"


using namespace std;
using namespace muduo;
using namespace muduo::net;


IUINT32 iclock()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return IUINT32(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

KcpDataSession::KcpDataSession(TcpConnectionRef ref)
	: conn_(ref)
{
	LOG_INFO << "kcp session created!";
}

KcpDataSession::~KcpDataSession()
{
	if(!kcp_)
	{
		LOG_INFO << "KcpDataSession destroyed without init!";
		return;
	}

	int remainBytes = ikcp_peeksize(kcp_);
	if(remainBytes > 0)
	{
		LOG_WARN << remainBytes << " bytes data left in kcp object!";
	}

	ikcp_flush(kcp_);
	ikcp_release(kcp_);
}

void KcpDataSession::init(uint32_t id)
{
    LOG_INFO << "session inited with id: " << id;
    assert(!kcp_);
    id_ = id;
    kcp_ = ikcp_create(id_, this);
    ikcp_setoutput(kcp_, &KcpTunnel::output);
    ikcp_nodelay(kcp_, 1, 10, 1, 1);

	while(!payloadList_.empty())
	{
		std::string& payload = payloadList_.front();
		int ret = ikcp_send(kcp_, payload.c_str(), (int)payload.length());
		if(ret != 0)
		{
			LOG_ERROR << "ikcp_send failed, ret: " << ret;
			return;
		}
		payloadList_.pop_front();
		LOG_INFO << "[" << id_ << "] A -> B, bytes: " << payload.length()
				 << ", content: " << payload.c_str();
	}
}

void KcpDataSession::update()
{
	ikcp_update(kcp_, iclock());
}

void KcpDataSession::sendToTunnel(const void *data, size_t len)
{
    if(!kcp_)
    {
        payloadList_.push_back(std::string((const char*)data, len));
    }
	else
	{
		int ret = ikcp_send(kcp_, (const char*)data, (int)len);
		if(ret != 0)
		{
			LOG_ERROR << "ikcp_send failed, ret: " << ret;
			return;
		}

		LOG_INFO << "[" << id_ << "] A -> B, bytes: " << len
				 << ", content: " << string((char*)data, len).c_str();
	}


	//ikcp_flush(kcp_);
}

int KcpDataSession::recvFromTunnel(const std::string &input)
{
	int inputRet = ikcp_input(kcp_, input.data(), (long)input.length());
	if(inputRet < 0)
	{
		LOG_WARN << "input failed, ret: " << inputRet;
		return -1;
	}

	int len = ikcp_peeksize(kcp_);
	if(len <= 0)
	{
	    if(input.length() != 24)
        {
            LOG_WARN << "peek found no output";
        }
		return 0;
	}

	string output;
	output.resize((size_t)len, '\0');
	int ret = ikcp_recv(kcp_, (char*)output.c_str(), len);
	assert(ret >= 0);

	TcpConnectionPtr connPtr = conn_.lock();
	assert(connPtr);
	connPtr->send(output.c_str(), len);

	LOG_INFO << "[" << id_ << "] B -> A, bytes: " << len << ", content: " << output.c_str();
	return 1;
}
void KcpDataSession::forceConnClose() {
  TcpConnectionPtr connPtr = conn_.lock();
  assert(connPtr);
  connPtr->forceClose();
}
