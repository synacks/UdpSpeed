#include <muduo/base/Logging.h>
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpClient.h"
#include <list>


using namespace std;
using namespace muduo;
using namespace muduo::net;

#define DATA_LEN 4096

void onConnection(const TcpConnectionPtr& conn)
{
	if(conn->connected())
	{
		std::string data(DATA_LEN, 'a');
		conn->send(data);
	}
	else
	{
		LOG_INFO << "disconnected";
	}
}

uint64_t g_count = 0;

void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
{
	if(buf->readableBytes() < DATA_LEN)
	{
			LOG_DEBUG << "recv bytes: " << buf->readableBytes();
	}
	else
	{
    	conn->send(buf->peek(), DATA_LEN);
    	buf->retrieve(DATA_LEN);
    	LOG_INFO << "sent message count: " << ++g_count;
	}
}

std::string g_server = "127.0.0.1";
uint16_t g_port = 6666;

class EchoClient;
typedef std::shared_ptr<EchoClient> EchoClientPtr;

std::list<EchoClientPtr> g_clients;

EventLoop* g_loop = nullptr;

class EchoClient : public enable_shared_from_this<EchoClient>
{
public:
		EchoClient(EventLoop* loop)
			: client_(loop, InetAddress(g_server.c_str(), g_port), "EchoClient")
			, roundCount_(0)
			, datalen_(0)
		{
		}

		~EchoClient()
		{
			LOG_INFO << "~EchoClient";
		}

		void connect()
		{
			client_.setConnectionCallback(std::bind(&EchoClient::onConnection, shared_from_this(), _1));
			client_.setMessageCallback(std::bind(&EchoClient::onMessage, shared_from_this(), _1, _2, _3));
			client_.connect();
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
				datalen_ =(uint32_t)rand();
				datalen_ = datalen_ % 4096 + 1;
				std::string data(datalen_, 'a');
				conn->send(data);
				LOG_INFO << "new client established: " << conn->name();
			}
			else
			{
				LOG_INFO << client_.name() << " disconnected";
			}
		}

		void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
		{
			if(buf->readableBytes() < datalen_)
			{
				LOG_DEBUG << "recv bytes: " << buf->readableBytes();
			}
			else
			{
				std::string originData(datalen_, 'a');
				assert(0 == memcmp(buf->peek(), originData.c_str(), datalen_));
				conn->send(buf->peek(), (int)datalen_);
				buf->retrieve(datalen_);
				++roundCount_;
				//LOG_INFO << "sent message count: " << ++roundCount_;
			}
		}

		uint32_t getRoundCount()
		{
			return roundCount_;
		}

private:
		TcpClient client_;
		uint32_t roundCount_;
		size_t datalen_;
};


void onTimer()
{
	for(auto it = g_clients.begin(); it != g_clients.end(); ++it)
	{
		if((*it)->getRoundCount() > (((uint32_t)rand()) % 10))
		{
			(*it)->disconnect();
			EchoClientPtr cli = make_shared<EchoClient>(g_loop);
			cli->connect();
			(*it) = cli;
		}
	}
}

int main()
{

	EventLoop loop;
	g_loop = &loop;

	while(g_clients.size() < 50)
	{
		auto cli = make_shared<EchoClient>(g_loop);
		cli->connect();
		g_clients.push_back(cli);
	}

	loop.runEvery(0.5, onTimer);

	loop.loop();
}
