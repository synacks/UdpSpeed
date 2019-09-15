//
// Created by leo on 9/8/19.
//

#include <muduo/base/Logging.h>
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpClient.h"
#include <list>

using namespace std;
using namespace muduo;
using namespace muduo::net;


std::string g_server = "127.0.0.1";
uint16_t g_port = 6666;

class DownloadClient;
typedef std::shared_ptr<DownloadClient> DownloadClientPtr;

std::list<DownloadClientPtr> g_clients;

EventLoop* g_loop = nullptr;

class DownloadClient : public enable_shared_from_this<DownloadClient>
{
public:
		DownloadClient(EventLoop* loop)
						: client_(loop, InetAddress(g_server.c_str(), g_port), "DownloadClient")
						, totalKB_(0)
						, prevKB_(0)
						, datalen_(0)
						, count_(0)
						, loop_(loop)
		{
		}

		~DownloadClient()
		{
			LOG_INFO << "~DownloadClient";
		}

		void connect()
		{
			client_.setConnectionCallback(std::bind(&DownloadClient::onConnection, shared_from_this(), _1));
			client_.setMessageCallback(std::bind(&DownloadClient::onMessage, shared_from_this(), _1, _2, _3));
			client_.connect();
			loop_->runEvery(3.0, std::bind(&DownloadClient::onTimer, shared_from_this()));
		}

		void disconnect()
		{
			client_.setConnectionCallback(defaultConnectionCallback);
			client_.setMessageCallback(defaultMessageCallback);
			client_.disconnect();
		}

		void onConnection(const TcpConnectionPtr& conn)
		{
			if(conn->connected())
			{
				LOG_INFO << "connected to " << g_server << ":" << g_port;
				conn->send("abc", 3);
			}
			else
			{
				LOG_INFO << "disconnected from " << g_server << ":" << g_port;
			}
		}

		void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
		{
			totalKB_ += buf->readableBytes() * 1.0 / 1024;
			buf->retrieveAll();
		}

		void onTimer()
		{
			count_++;
			LOG_INFO << "download speed: " << (totalKB_ - prevKB_) / 3  << " KB/s"
							 << ", avg speed: " << totalKB_ / 3 / count_ << " KB/s";
			prevKB_ = totalKB_;

		}


private:
		TcpClient client_;
		double totalKB_;
		double prevKB_;
		size_t datalen_;
		size_t count_;
		EventLoop* loop_;
};



int main(int argc, char* argv[])
{
	if(argc != 3)
	{
		printf("Usage: %s <dest_ip> <dest_port>\n", argv[0]);
		return -1;
	}

	g_server = argv[1];
	g_port = (uint16_t)atoi(argv[2]);

	EventLoop loop;
	g_loop = &loop;

	while(g_clients.size() < 1)
	{
		auto cli = make_shared<DownloadClient>(g_loop);
		cli->connect();
		g_clients.push_back(cli);
	}

	loop.loop();
}
