#include <iostream>
#include "KcpProxy.h"
#include "KcpTunnel.h"
#include "Option.h"


Option g_option;

int main(int argc, char* argv[])
{
	if(argc != 3)
	{
		printf("Usage: %s <serverip> <serverport>\n", argv[0]);
		return -1;
	}

	g_option.serverIp = argv[1];
	g_option.serverPort = uint16_t(atoi(argv[2]));

	KcpTunnel::init();

	KcpProxy proxy;
	proxy.run();
	return 0;
}