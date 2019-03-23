#include <stdio.h>
#include "KcpServer.h"
#include "Option.h"


Option g_option;

int main(int argc, char* argv[])
{
	if(argc != 3)
	{
		printf("Usage: %s <ip> <port>\n", argv[0]);
		return -1;
	}

	g_option.destIp = argv[1];
	g_option.destPort = (uint16_t)atoi(argv[2]);

	KcpServer server;
	server.run();

	return 0;
}