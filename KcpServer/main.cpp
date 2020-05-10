#include <stdio.h>
#include "KcpServer.h"
#include "Option.h"


Option g_option;

//测试用例：
//1. B重启了，C没有重启，B的第一个会话ID为１，与C中的ID为１的会话ID对应不起来。

//ID管理
//ID由C生成，B需要向C申请ID，申请时机是B接收到新的TCP连接时
//B发现TCP连接断开时，释放对应会话ID给C。（另外，C在该ID上长时间没有收到B的消息时，也可以主动回收该ID）

//会话的创建和销毁
//ID为0的会话为管理会话，在该会话上实现可靠的控制信令传输。信令的传输是双向的，即B可以向C发送信令，C也可以向B发送信令。
//目前的信令有以下几个：
//1.创建会话，该信令只能由B向C发送，B通过该信令向C申请会话ID。
//2.销毁会话，该信令可以双向发送，B通知C销毁会话，C也可以通知B销毁会话，会话销毁的同时，会话ID也会被回收。

//重启管理
//TODO: C重启后，分配的ID应当与之前的ID有所区别，避免旧有的会话影响到现有的会话
int main(int argc, char* argv[])
{
	if(argc != 3)
	{
		printf("Usage: %s <dest_ip> <dest_port>\n", argv[0]);
		return -1;
	}

	g_option.destIp = argv[1];
	g_option.destPort = (uint16_t)atoi(argv[2]);

	KcpServer server;
	server.run();

	return 0;
}