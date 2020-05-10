//
// Created by leo on 2020/5/7.
//

#ifndef PROXY_KCP_CONTROLSESSION_H
#define PROXY_KCP_CONTROLSESSION_H

#include "ikcp.h"
#include <string>
#include <vector>

typedef int (*FnKcpOutput)(const char *buf, int len, ikcpcb *kcp, void *user);

enum {
  CMD_CREATE_SESSION = 1,
  CMD_DESTROY_SESSION = 2,
  RSP_CREATE_SESSION = 3
};

class ControlSession {
public:
  ControlSession(FnKcpOutput cb);
  ~ControlSession();

  void sendMsg(const char* cmd, size_t len);
  int  parseMsg(const char* packet, size_t packetlen, std::vector<std::string>& aryMsg);

protected:
  ikcpcb* kcp_ = nullptr;
  std::string recvBuffer_;
};

#endif //PROXY_KCP_CONTROLSESSION_H
