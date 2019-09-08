//
// Created by leo on 9/8/19.
//

#include <muduo/net/EventLoop.h>
#include <signal.h>
#include "muduo/net/TcpServer.h"
#include "muduo/base/Logging.h"

using namespace muduo;
using namespace muduo::net;

std::string g_data(40*1024, 'a');

void onConnection(const TcpConnectionPtr& conn)
{
	LOG_INFO << "connection " << (conn->connected() ? "established " : "disconnected ")
	         << conn->peerAddress().toIpPort()
	         << " -> "
	         << conn->localAddress().toIpPort();
	conn->send(g_data.c_str(), (int)g_data.length());
}

void onWriteComplete(const TcpConnectionPtr& conn)
{
	conn->send(g_data.c_str(), (int)g_data.length());
	LOG_INFO << "send " << g_data.length() << " bytes data";
}

void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
{
	LOG_INFO << conn->peerAddress().toIpPort()
	         << " -> "
	         << conn->localAddress().toIpPort()
	         << ", bytes: " << buf->readableBytes();
	buf->retrieveAll();
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

	TcpServer server(loopPtr.get(), InetAddress("0.0.0.0", 6669), "server", TcpServer::kReusePort);
	server.setConnectionCallback(std::bind(&onConnection, _1));
	server.setMessageCallback(std::bind(&onMessage, _1, _2, _3));
	server.setWriteCompleteCallback(std::bind(&onWriteComplete, _1));
	server.start();

	LOG_INFO << "server started at 0.0.0.0:6669";

	loopPtr->loop();

	return 0;
}