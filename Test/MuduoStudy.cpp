//
// Created by leo on 2020/5/10.
//
#include <muduo/base/Logging.h>
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/InetAddress.h"

using namespace muduo;
using namespace muduo::net;


class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(EventLoop* loop)
        : loop_(loop)
    {
        InetAddress remote("127.0.0.1", (uint16_t)9999);
        client_ = std::make_shared<TcpClient>(loop_, remote, "tcpcli");
    }

    ~ClientSession() {
        log_info << "client session destroyed: " << this;
    }

    void connectSvr() {
        client_->setConnectionCallback(std::bind(&ClientSession::onConnection, shared_from_this(), _1));
        client_->setMessageCallback(std::bind(&ClientSession::onMessage, this, _1, _2, _3));
        client_->connect();
        loop_->runAfter(3, std::bind(&ClientSession::onCheckConnection, shared_from_this()));
    }

    void disconnectSvr() {
        client_.reset();
    }

    void onConnection(const TcpConnectionPtr& conn) {
        if(conn->connected()) {
            log_info << "connected!";
        } else {
            log_info << "disconnected!";
        }
    }

    void onCheckConnection() {


    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp ts) {
        log_info << "get message, bytes: " << buf->readableBytes();
        buf->retrieveAll();
    }

private:
    EventLoop* loop_;
    std::shared_ptr<TcpClient> client_;
};


std::shared_ptr<ClientSession> g_sess;


void destroySession() {
    g_sess->disconnectSvr();
    g_sess.reset();
    log_info << "destroySession callled!";
}

void exitLoop(EventLoop* l) {
    l->quit();
}


int main() {
    EventLoop loop;
    g_sess = std::make_shared<ClientSession>(&loop);
    g_sess->connectSvr();
    loop.runAfter(1, std::bind(&destroySession));
    //loop.runAfter(3, std::bind(&exitLoop, &loop));
    loop.loop();

    return 0;
}