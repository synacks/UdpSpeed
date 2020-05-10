//
// Created by leo on 2/5/19.
//

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <muduo/base/Logging.h>
#include <sstream>
#include <memory>
#include "KcpServer.h"

using namespace std;

int g_udpServer = -1;

KcpServer::KcpServer()
    : controlSess_(&KcpServerSession::output)
{
    id_ = (uint32_t)time(NULL) + 24 * 3600;
    g_udpServer = createUdpTunnel();
    udpServerChan_ = make_shared<Channel>(&loop_, g_udpServer);
}

KcpServer::~KcpServer() {
}

void KcpServer::run() {
    udpServerChan_->setReadCallback(std::bind(&KcpServer::onUdpTunnelReadable, this, _1));
    udpServerChan_->enableReading();
    loop_.runEvery(0.01, std::bind(&KcpServer::onUpdateKcpTimer, this));
    loop_.runEvery(15, std::bind(&KcpServer::onCheckDeadSessionTimer, this));
    loop_.loop();
}

int KcpServer::createUdpTunnel() {
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    assert(fd >= 0);
    sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = inet_addr("0.0.0.0");
    local.sin_port = htons(6667);
    assert(0 == bind(fd, (sockaddr *)&local, sizeof(local)));
    return fd;
}


void KcpServer::onUdpTunnelReadable(Timestamp) {
    char buf[4096] = {0};
    sockaddr_in remote;
    socklen_t remoteLen = sizeof(remote);
    ssize_t bytes = recvfrom(g_udpServer, buf, sizeof(buf), 0, (sockaddr *)&remote, &remoteLen);
    if(bytes < 0) {
        LOG_ERROR << "recv from udpserver failed, errno: " << errno;
        return;
    }

    LOG_INFO << "remote address: " << inet_ntoa(remote.sin_addr) << ":" << ntohs(remote.sin_port);

    dispatchKcpPacket(buf, bytes, &remote);
}

void KcpServer::dispatchKcpPacket(const char *packet, size_t bytes, sockaddr_in *remote) {
    IUINT32 sn = ikcp_getsn(packet, bytes);
    if(sn == -1) {
        LOG_ERROR << "invalid kcp packet: cannot get sn from kcp packet!";
        return;
    }

    IUINT32 id = ikcp_getconv(packet);
    if(id == 0) {
        std::vector<std::string> aryMsg;
        controlSess_.parseMsg(packet, bytes, aryMsg);
        for(auto msg : aryMsg) {
            handleControlMessage(msg, remote);
        }

        return;
    }

    //获取已经存在的session
    auto it = sessionMap_.find(id);
    if(it != sessionMap_.end()) {
        if(sn == 0) {
            LOG_WARN << "sn should not be 0 while found live session, maybe duplicate packet!";
        }
        it->second->recvFromTunnel(packet, bytes);
        return;
    }

}

void KcpServer::onUpdateKcpTimer() {
    for(auto it = sessionMap_.begin(); it != sessionMap_.end(); ++it) {
        it->second->updateKcp();
    }
}

void KcpServer::onCheckDeadSessionTimer() {
    auto it = sessionMap_.begin();
    while(it != sessionMap_.end()) {
        auto cur = it++;
        if(cur->second->isIdle()) {
            LOG_INFO << cur->second->getIdStr() << " found dead session, erase it!";
            //cur->second->disconnectFromSvr();
            sessionMap_.erase(cur);
        }
    }
}

void KcpServer::notifyPeerException(uint32_t sessId, sockaddr_in *remote) {
    LOG_WARN << "notify invalid session id: " << sessId;
    uint32_t packet[2] = {0, htonl(sessId)};
    sendto(g_udpServer, packet, sizeof(packet), 0, (sockaddr *)(remote), sizeof(sockaddr_in));
}

void KcpServer::handleControlMessage(const std::string &controlMsg, const sockaddr_in *remote) {
    assert(!controlMsg.empty());

    int cmd = controlMsg[0];
    if(cmd == CMD_CREATE_SESSION) {
        uint32_t id = id_++;
        if(id == 0) { id = id_++; } //id不能为0
        LOG_INFO << "create new kcp session, session id: " << id;
        KcpServerSessionPtr sess = make_shared<KcpServerSession>(&loop_, id, remote);
        sess->connectSvr();
        sessionMap_[id] = sess;
        char msg[5] = {RSP_CREATE_SESSION};
        uint32_t idNetOrder = htonl(id);
        memcpy(msg + 1, &idNetOrder, 4);
        controlSess_.sendMsg(msg, 5);
    } else if(cmd == CMD_DESTROY_SESSION) {
        uint32_t id = 0;
        memcpy(&id, controlMsg.c_str() + 1, 4);
        id = ntohl(id);
        auto it = sessionMap_.find(id);
        if(it != sessionMap_.end()) {
            it->second->disconnectSvr();
            sessionMap_.erase(it);
        } else {
            LOG_WARN << "session not found, session id: " << id;
        }
    } else {
        assert(0);
    }
}
