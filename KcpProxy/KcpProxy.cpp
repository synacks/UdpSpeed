//
// Created by leo on 2/4/19.
//

#include "KcpProxy.h"
#include "muduo/net/InetAddress.h"
#include "muduo/base/Logging.h"
#include <functional>
#include "kcp/ikcp.h"
#include "KcpTunnel.h"

using namespace std;

KcpProxy::KcpProxy()
	: server_(&loop_, InetAddress(6665), "KcpProxy", TcpServer::kReusePort), kcpChan_(&loop_, KcpTunnel::g_udpSock)
	, controlSess_(&KcpTunnel::output)
{}

void KcpProxy::run() {
  kcpChan_.setReadCallback(std::bind(&KcpProxy::onKcpTunnelMessage, this, _1));
  kcpChan_.enableReading();

  server_.setThreadNum(0);
  server_.setConnectionCallback(std::bind(&KcpProxy::onConnection, this, placeholders::_1));
  server_.setMessageCallback(std::bind(&KcpProxy::onMessage, this, _1, _2, _3));
  server_.start();
  loop_.runEvery(0.01, std::bind(&KcpProxy::onUpdateTunnelTimer, this));
  loop_.loop();
}

void KcpProxy::onConnection(const TcpConnectionPtr &conn) {
  LOG_INFO << "connection " << (conn->connected() ? "established " : "disconnected ")
		   << conn->peerAddress().toIpPort()
		   << " -> "
		   << conn->localAddress().toIpPort();

  if (conn->connected()) {
	KcpDataSessionPtr sess = make_shared<KcpDataSession>(conn);
	conn->setContext(sess);
	uninitedSessList_.push_back(sess);

	char createSessionCmd = CMD_CREATE_SESSION;
	controlSess_.sendMsg(&createSessionCmd, 1);
  } else {
	KcpDataSessionPtr sess = boost::any_cast<KcpDataSessionPtr>(conn->getContext());

	//清理该session在其他对象中的引用
	for (auto it = uninitedSessList_.begin(); it != uninitedSessList_.end(); ++it) {
	  if (it->lock() == sess) {
		uninitedSessList_.erase(it);
		break;
	  }
	}

	if (sess->id() != 0) {
	  sessionMap_.erase(sess->id());
	  notifyDeadSessionId(sess->id());
	}
  }
}

void KcpProxy::onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp) {
  KcpDataSessionPtr sess = boost::any_cast<KcpDataSessionPtr>(conn->getContext());
  sess->sendToTunnel(buf->peek(), buf->readableBytes());
  buf->retrieveAll();
}

void KcpProxy::onKcpTunnelMessage(Timestamp) {
  char buf[4096] = {0};
  ssize_t ret = read(KcpTunnel::g_udpSock, buf, sizeof(buf));
  if (ret < 0) {
	LOG_ERROR << "read error, errno: " << errno;
	return;
  }

  IUINT32 sessId = ikcp_getconv(buf);

  //sessId为0表示为管理会话
  if (sessId == 0) {
	//处理创建会话的回复
	std::vector<std::string> aryMsg;
	controlSess_.parseMsg(buf, ret, aryMsg);
	for(auto msg : aryMsg) {
	  handleControlMessage(msg);
	}
	return;
  }

  auto it = sessionMap_.find(sessId);
  if (sessionMap_.end() == it) {
	notifyDeadSessionId(sessId);
	return;
  }

  LOG_INFO << "[" << sessId << "] C -> B, bytes: " << ret;

  KcpDataSessionPtr sess = it->second.lock();
  assert(sess);
  if (sess->recvFromTunnel(std::string(buf, (size_t)ret)) < 0) {
	LOG_WARN << "recv from tunnel failed";
	return;
  }
}

void KcpProxy::onUpdateTunnelTimer() {
  for (auto it = sessionMap_.begin(); it != sessionMap_.end(); ++it) {
	auto sess = it->second.lock();
	assert(sess);
	sess->update();
  }
}

void KcpProxy::handleControlMessage(const std::string &controlMsg) {
  assert(!controlMsg.empty());

  uint8_t cmd = controlMsg[0];
  if (cmd == RSP_CREATE_SESSION) {
	uint32_t newSessId = 0;
	memcpy(&newSessId, controlMsg.c_str() + 1, 4);
	newSessId = ntohl(newSessId);
	LOG_INFO << "get new session id: " << newSessId;
	//将该会话ID给予未初始化Session列表中的第一个
	//uninitedSessList_中的会话对象与连接的生命周期一致
	if (!uninitedSessList_.empty()) {
	  KcpDataSessionPtr sess = uninitedSessList_.front().lock();
	  assert(sess);
	  sess->init(newSessId);
	  uninitedSessList_.pop_front();
	  sessionMap_[newSessId] = sess;
	} else {
	  //如果未初始化的会话列表未空，则表示在收到此回复之前，A已经断开了连接
	  LOG_INFO << "connection lost, the new created session id will not be used!";
	  notifyDeadSessionId(newSessId);
	}
  } else if (cmd == CMD_DESTROY_SESSION) {
    uint32_t sessId = 0;
    memcpy(&sessId, controlMsg.c_str() + 1, 4);
    sessId = ntohl(sessId);
    if(sessionMap_.find(sessId) != sessionMap_.end()) {
      auto sess = sessionMap_[sessId].lock();
      assert(sess);
      sess->forceConnClose();
    } else {
      LOG_INFO << "session already closed, session id: " << sessId;
    }
  } else {
	LOG_ERROR << "unknown response cmd: " << cmd;
  }
}

void KcpProxy::notifyDeadSessionId(uint32_t id) {
  char cmd[5] = {0};
  cmd[0] = CMD_DESTROY_SESSION;
  id = htonl(id);
  memcpy(&cmd[1], &id, 4);
  controlSess_.sendMsg(cmd, sizeof(cmd));
}
