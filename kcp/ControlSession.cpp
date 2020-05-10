//
// Created by leo on 2020/5/7.
//

#include "ControlSession.h"
#include <muduo/base/Logging.h>
#include <vector>

size_t getMsgLength(uint8_t cmd) {
  if(cmd == CMD_CREATE_SESSION) {
    return 1;
  } else if (cmd == CMD_DESTROY_SESSION) {
    return 5;
  } else if (cmd == RSP_CREATE_SESSION) {
    return 5;
  } else {
    LOG_ERROR << "invalid control cmd: " << cmd;
    return 0;
  }
}

ControlSession::ControlSession(FnKcpOutput cb) {
  kcp_ = ikcp_create(0, nullptr);
  ikcp_setoutput(kcp_, cb);
  ikcp_nodelay(kcp_, 1, 10, 1, 1);
}


ControlSession::~ControlSession() {
  ikcp_release(kcp_);
}

void ControlSession::sendMsg(const char *cmd, size_t len) {
  int ret = ikcp_send(kcp_, cmd, len);
  if (ret < 0) {
	LOG_ERROR << "ikcp_send failed, ret: " << ret;
  }
}

int ControlSession::parseMsg(const char *packet, size_t packetlen, std::vector<std::string>& aryMsg) {
  int inputRet = ikcp_input(kcp_, packet, (long)packetlen);
  if (inputRet < 0) {
	LOG_WARN << "input failed, ret: " << inputRet;
	return -1;
  }

  int len = ikcp_peeksize(kcp_);
  if (len == 0) {
	return 0;
  }

  std::string payload;
  payload.resize((size_t)len);
  int ret = ikcp_recv(kcp_, (char *)payload.c_str(), len);
  assert(ret >= 0);
  payload.resize(ret);

  recvBuffer_.append(payload);

  aryMsg.clear();
  while (recvBuffer_.empty()) {
	uint8_t cmd = recvBuffer_[0];
	size_t cmdLength = getMsgLength(cmd);
	assert(cmdLength > 0);
	if (recvBuffer_.length() < cmdLength) {
	  break;
	}
	aryMsg.push_back(recvBuffer_.substr(0, cmdLength));
	recvBuffer_.erase(0, cmdLength);
  }

  return 1;
}