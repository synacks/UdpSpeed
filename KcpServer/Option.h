//
// Created by leo on 2/8/19.
//

#ifndef PROXY_OPTION_H
#define PROXY_OPTION_H

#include <string>
#include <stdint.h>


struct Option
{
	std::string destIp;
	uint16_t destPort;
};

extern Option g_option;


#endif //PROXY_OPTION_H
