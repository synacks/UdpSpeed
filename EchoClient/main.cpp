#include <muduo/base/Logging.h>
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpClient.h"


using namespace std;
using namespace muduo;
using namespace muduo::net;


void onConnection(const TcpConnectionPtr& conn)
{
	if(conn->connected())
	{
		std::string data(1024, 'a');
		conn->send(data);
	}
	else
	{
		LOG_INFO << "disconnected";
	}
}

void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
{
	LOG_INFO << "recv bytes: " << buf->readableBytes();
	conn->send(buf);
}

int main()
{
	EventLoop loop;

	TcpClient client(&loop, InetAddress("127.0.0.1", 6666), "");

	client.setConnectionCallback(std::bind(&onConnection, _1));
	client.setMessageCallback(std::bind(&onMessage, _1, _2, _3));
	client.connect();

	loop.loop();
}
