#include <muduo/net/EventLoop.h>
#include <signal.h>
#include "muduo/net/TcpServer.h"
#include "muduo/base/Logging.h"

using namespace muduo;
using namespace muduo::net;

void onConnection(const TcpConnectionPtr& conn)
{
	LOG_INFO << "connection " << (conn->connected() ? "established " : "disconnected ")
			 << conn->peerAddress().toIpPort()
			 << " -> "
			 << conn->localAddress().toIpPort();
}

void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
{
	LOG_INFO << conn->peerAddress().toIpPort()
			 << " -> "
			 << conn->localAddress().toIpPort()
			 << ", bytes: " << buf->readableBytes();
	conn->send(buf);
}

std::shared_ptr<EventLoop> g_loop;

void interruptHandler(int)
{
	g_loop->quit();
}

int main()
{
	signal(SIGINT, interruptHandler);

	std::shared_ptr<EventLoop> loopPtr = std::make_shared<EventLoop>();
	g_loop = loopPtr;

	TcpServer server(loopPtr.get(), InetAddress("127.0.0.1", 6668), "server", TcpServer::kReusePort);
	server.setConnectionCallback(std::bind(&onConnection, _1));
	server.setMessageCallback(std::bind(&onMessage, _1, _2, _3));
	server.start();
	loopPtr->loop();

	return 0;
}