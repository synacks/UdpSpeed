//
// Created by leo on 2020/5/5.
//

#ifndef PROXY_KCPCONTROLSESSION_H
#define PROXY_KCPCONTROLSESSION_H

#include "kcp/ikcp.h"

#define CMD_CREATE_SESSION 		1
#define CMD_DESTROY_SESSION 	2

class KcpControlSession
{
public:
	KcpControlSession();

	void sendCmd(const char* cmd, size_t len);
	int  parseResp(const char* packet, size_t len, std::string& resp);

private:
	ikcpcb* kcp_ = nullptr;
};


#endif //PROXY_KCPCONTROLSESSION_H
