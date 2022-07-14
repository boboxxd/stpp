#include <signal.h>
#include <vector>
#include "core/coroutine.hpp"
#include <ostream>
#include <sstream>
#include <iostream>
#include "core/error.hpp"
#include "core/net.hpp"
#include <signal.h>
#define ENABLE_SIMPLE_GLOG 1
#if(ENABLE_SIMPLE_GLOG)
#include "core/logging.hpp"
#endif

//template<typename ConnPtr>
//class SessionManagerImpl {
//public:
//	void addConn(ConnPtr cli) {
//		sesses.push_back(cli);
//	}
//
//	void removeConn(ConnPtr cli) {
//		LOG(TRACE) << "removeConn";
//		stopsesses.push_back(cli);
//		sesses.erase(std::find(sesses.begin(), sesses.end(), cli));
//	}
//
//	size_t count() const { return sesses.size(); }
//
//	st::error_t start() {
//		co_ = st::coroutine(&::SessionManagerImpl<ConnPtr>::run, this);
//		return error_ok;
//	}
//
//private:
//	void run() {
//		while (!exit) {
//			if (stopsesses.empty())
//				st::this_coroutine::sleep_for(std::chrono::seconds(1));
//			stopsesses.clear();
//		}
//	}
//private:
//	bool exit = false;
//	std::vector<ConnPtr> sesses;
//	std::vector<ConnPtr> stopsesses;
//	st::coroutine co_;
//};
//
//class TcpConnection;
//using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
//using SessionManager = SessionManagerImpl<TcpConnectionPtr>;
//
//class TcpConnection:public std::enable_shared_from_this<TcpConnection> {
//public:
//	TcpConnection(st::netfd_t fd, SessionManager* mgr) :fd_(fd),mgr_(mgr) {
//		
//	}
//
//	~TcpConnection() {
//		exit_ = true;
//		st_netfd_close(fd_);
//		LOG(TRACE) << "~TcpConnection";
//	}
//
//	st::error_t start() {
//		mgr_->addConn(shared_from_this());
//		co_ = st::coroutine(&::TcpConnection::run, this);
//		return error_ok;
//	}
//private:
//	void run() {
//		while (!exit_) {
//			char buf[1024] = {0};
//			int n = st_read(fd_, buf, 1024, ST_UTIME_NO_TIMEOUT);
//			if (n == 0) {
//				LOG(TRACE) << "peer connection closed!";
//				break;
//			}else if(n < 0) {
//				if (errno == ETIME)
//					st::this_coroutine::yield();
//				if (errno == EINTR) {
//					LOG(TRACE) << "coroutine interrupted!";
//					break;
//				}
//			}else{
//				LOG(TRACE) << std::string(buf);
//			}
//		}
//		LOG(TRACE) << "TcpConnection run exit!";
//		mgr_->removeConn(shared_from_this());
//	}
//private:
//	bool exit_ = false;
//	st::netfd_t fd_;
//	SessionManager* mgr_;
//	st::coroutine co_;
//};

st::condition_variable  con;

void sig_handler(int signo) {
	con.notify_all();
}

int main() {
	signal(SIGINT, sig_handler);
	st::enable_coroutine();
	st::LogStream::setLogLevel(TRACE);

	st::TcpServer svr("0.0.0.0", 33332);
	auto err = svr.start();
	if (err) {
		LOG(ERROR) << err->what();
		return -1;
	}

	con.wait();
	svr.stop();
	LOG(INFO) << "program exit normally ...";
	return 0;
}