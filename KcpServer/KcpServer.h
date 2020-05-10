//
// Created by leo on 2/5/19.
//

#ifndef PROXY_KCPSERVER_H
#define PROXY_KCPSERVER_H

#include <muduo/net/Channel.h>
#include "muduo/net/EventLoop.h"
#include "KcpServer/KcpServerSession.h"
#include "kcp/ControlSession.h"
#include <set>

using namespace muduo;
using namespace muduo::net;

typedef std::set<IUINT32> DeadSessionSet;

class KcpServer {
public:
  KcpServer();
  ~KcpServer();

public:
  void run();

private:
  int createUdpTunnel();
  void onUdpTunnelReadable(Timestamp);
  void onUpdateKcpTimer();
  void onCheckDeadSessionTimer();

  void dispatchKcpPacket(const char *packet, size_t len, sockaddr_in *remote);
  void notifyPeerException(uint32_t sessId, sockaddr_in *remote);
  void handleControlMessage(const std::string& controlMsg, const sockaddr_in* remote);

private:
  EventLoop loop_;
  std::shared_ptr<Channel> udpServerChan_;
  std::map<IUINT32, KcpServerSessionPtr> sessionMap_;
  ControlSession controlSess_;
  uint32_t id_ = 1;
};

#endif //PROXY_KCPSERVER_H
