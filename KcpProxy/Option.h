//
// Created by leo on 2/8/19.
//
#ifndef __KCP_PROXY_OPTION_H__
#define __KCP_PROXY_OPTION_H__

#include <string>
#include <stdint.h>

struct Option
{
	std::string serverIp;
	uint16_t serverPort;
};

extern Option g_option;

#endif