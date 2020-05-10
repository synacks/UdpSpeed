//
// Created by leo on 2/6/19.
//

#include <muduo/base/Logging.h>
#include "KcpServerSession.h"
#include <errno.h>
#include "Option.h"
#include <memory>
#include <sstream>

extern int g_udpServer;

using namespace std;

static IUINT32 iclock() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return IUINT32(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}


int KcpServerSession::output(const char *buf, int len, ikcpcb *kcp, void *user) {
    KcpServerSession *sess = (KcpServerSession *)user;
    assert(sess);

    assert(g_udpServer >= 0);
    assert(len > 0);
    ssize_t bytes = 0;

    do {
        bytes = sendto(g_udpServer, buf, (size_t)len, 0,
                       (const sockaddr *)sess->getTunnelPeer(), sizeof(sockaddr));
    } while(bytes < 0 && errno == EAGAIN);

    assert(bytes == len);
    LOG_INFO << sess->getIdStr() << " C -> B, bytes: " << bytes;
    return (int)bytes;
}

KcpServerSession::KcpServerSession(EventLoop *loop, IUINT32 id, const sockaddr_in *remote)
    : loop_(loop)
    , client_(make_shared<TcpClient>(loop, InetAddress(g_option.destIp, g_option.destPort), "tcpclient"))
{
    kcp_ = ikcp_create(id, this);
    ikcp_nodelay(kcp_, 1, 10, 1, 1);
    ikcp_setoutput(kcp_, output);
    lastTunnelActiveTime_ = Timestamp::now();
    memcpy(&remoteAddr_, remote, sizeof(remoteAddr_));
}

KcpServerSession::~KcpServerSession() {
    LOG_INFO << getIdStr() << " server session destroying!";
    ikcp_flush(kcp_);
    ikcp_release(kcp_);
}

void KcpServerSession::connectSvr(SessionConnectionLostCallback cb) {
    //用share_from_this的原因是：如果用this，Session对象会早于该回调被析构
    client_->setConnectionCallback(std::bind(&KcpServerSession::onConnection, shared_from_this(), _1));
    client_->setMessageCallback(std::bind(&KcpServerSession::onMessage, shared_from_this(), _1, _2, _3));
    client_->connect();
    loop_->runAfter(3, std::bind(&KcpServerSession::onCheckConnection, shared_from_this()));
}

void KcpServerSession::disconnectSvr() {
    client_.reset();
}

void KcpServerSession::recvFromTunnel(const char *packet, size_t len) {
    IUINT32 sn = ikcp_getsn(packet, len);
    LOG_INFO << getIdStr() << " B -> C, bytes: " << len << ", sn: " << sn;

    std::string payload;
    extractTunnelPayload(std::string(packet, (size_t)len), payload);
    if(payload.empty()) {
        LOG_DEBUG << "extract payload from tunnel packet failed";
        return;
    }

    lastTunnelActiveTime_ = Timestamp::now();
    if(client_) {
        TcpConnectionPtr conn = client_->connection();
        if(conn) {
            conn->send(payload);
        } else {
            payloadList_.push_back(payload);
        }
    } else {
        log_error << "connection already lost!";
        return;
    }
}

void KcpServerSession::extractTunnelPayload(const std::string &input, std::string &output) {
    int inputRet = ikcp_input(kcp_, input.c_str(), input.length());
    if(inputRet < 0) {
        LOG_WARN << "get invalid kcp packet, ret: " << inputRet;
        return;
    }

    int peekSize = ikcp_peeksize(kcp_);
    if(peekSize <= 0) {
        if(input.length() != 24) {
            LOG_ERROR << "extract tunnel payload failed!";
        }
        return;
    }

    output.resize((size_t)peekSize, '\0');
    int recvRet = ikcp_recv(kcp_, (char *)output.data(), peekSize);
    if(recvRet <= 0) {
        LOG_WARN << "recv failed, ret: " << recvRet;
        return;
    }

    output.resize((size_t)recvRet);
    LOG_INFO << getIdStr() << " C -> D, bytes: " << recvRet;
}

void KcpServerSession::onConnection(const TcpConnectionPtr &conn) {
    if(conn->connected()) {
        LOG_TRACE << "connection established with " << conn->peerAddress().toIpPort();
        while(!payloadList_.empty()) {
            conn->send(payloadList_.front());
            LOG_TRACE << "found data cached while connecting dest server"
                      << ", data: " << &payloadList_.front()
                      << ", length: " << payloadList_.front().length();
            payloadList_.pop_front();
        }
    } else {
        LOG_INFO << "connection disconnected from " << conn->peerAddress().toIpPort();
        connLostCb_(kcp_->conv);
    }
}

void KcpServerSession::onCheckConnection() {
    //如果还没有连接上服务器，就放弃连接
    if(client_) {
        if(!client_->connection()) {
            client_.reset();
            connLostCb_(kcp_->conv);
        }
    }
}

void KcpServerSession::onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp) {
    int sendRet = ikcp_send(kcp_, buf->peek(), (int)buf->readableBytes());
    assert(sendRet == 0);
    LOG_INFO << getIdStr() << " D -> C, bytes: " << buf->readableBytes();
    buf->retrieveAll();
}

sockaddr_in *KcpServerSession::getTunnelPeer() {
    return &remoteAddr_;
}

void KcpServerSession::updateKcp() {
    ikcp_update(kcp_, iclock());
}

bool KcpServerSession::isIdle() {
    //长时间没有交互
    double delta = timeDifference(Timestamp::now(), lastTunnelActiveTime_);
    return delta > 1.0 * 3600;
}

string KcpServerSession::getIdStr() {
    ostringstream oss;
    oss << "[" << this << " " << kcp_->conv << "]";
    return oss.str();
}
