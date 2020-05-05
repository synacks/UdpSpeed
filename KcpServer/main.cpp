#include <stdio.h>
#include "KcpServer.h"
#include "Option.h"


Option g_option;

//测试用例：
//1. B重启了，C没有重启，B的第一个会话ID为１，与C中的ID为１的会话ID对应不起来。

//ID管理
//ID由B生成，生成方式为自增，范围是从0~0xFFFFFFFF, 0为特殊的管理通道ID，其他ID为数据通道ID。
//B的初始ID需要从C获得，C根据内部记录的ID情况给B分配一个初始ID
//C接收到数据包时，从数据包中的最前面4个字节中得到ID，根据数据包中的ID，做相应的处理：
//1.找到已有的有效会话，按照正常流程处理数据包
//2.发现ID已经无效，通B该ID已经无效了，并推荐一个有效的ID，防止B在无效ID上尝试多次
//3.发现ID为新的有效ID，创建一个新的会话

//C可以知道哪个ID是无效的，当检测到无效ID的时候，给B推荐一个新的有效ID。
//检测请求中的ID是否有效的方法：1.当请求中的ID在无效ID列表中；

//一方发现会话ID失效，需要立即通知到对端，且B和C在处理失效会话时，需要通知到TCP端

//ID管理2
//ID由C生成，B需要向C申请ID，申请时机是B接收到新的TCP连接时
//B发现TCP连接断开时，释放对应会话ID给C。（另外，C在该ID上长时间没有收到B的消息时，也可以主动回收该ID）

//会话的创建和销毁
//ID为0的会话为管理会话，在该会话上实现可靠的控制信令传输。信令的传输是双向的，即B可以向C发送信令，C也可以向B发送信令。
//目前的信令有以下几个：
//1.创建会话，该信令只能由B向C发送，B通过该信令向C申请会话ID。
//2.销毁会话，该信令可以双向发送，B通知C销毁会话，C也可以通知B销毁会话，会话销毁的同时，会话ID也会被回收。

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