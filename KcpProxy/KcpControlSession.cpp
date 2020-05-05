//
// Created by leo on 2020/5/5.
//

#include <muduo/base/Logging.h>
#include "KcpControlSession.h"
#include "KcpTunnel.h"

KcpControlSession::KcpControlSession()
{
	kcp_ = ikcp_create(0, nullptr);
	ikcp_setoutput(kcp_, &KcpTunnel::output);
	ikcp_nodelay(kcp_, 1, 10, 1, 1);
}

void KcpControlSession::sendCmd(const char* cmd, size_t len)
{
	int ret = ikcp_send(kcp_, cmd, len);
	if(ret < 0)
	{
		LOG_ERROR << "ikcp_send failed, ret: " << ret;
	}
}

int KcpControlSession::parseResp(const char* packet, size_t packetlen, std::string& resp)
{
	int inputRet = ikcp_input(kcp_, packet, (long)packetlen);
	if(inputRet < 0)
	{
		LOG_WARN << "input failed, ret: " << inputRet;
		return -1;
	}

	int len = ikcp_peeksize(kcp_);
	if(len == 0)
	{
		return 0;
	}

	resp.resize((size_t)len);
	int ret = ikcp_recv(kcp_, (char*)resp.c_str(), len);
	assert(ret >= 0);
	resp.resize(ret);
	return ret;
}




