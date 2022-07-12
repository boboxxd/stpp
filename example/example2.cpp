#include <signal.h>
#include <vector>
#include "core/coroutine.hpp"
#include <ostream>
#include <sstream>
#include <iostream>

#define ENABLE_SIMPLE_GLOG 1

#if(ENABLE_SIMPLE_GLOG)
#include "core/logging.hpp"
#endif
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include "core/error.hpp"
#include <netinet/in.h>
#include <sys/types.h>
#include<arpa/inet.h>
#include <vector>

#define ST_UTIME_NO_TIMEOUT ((st_utime_t) -1LL)
#define SERVER_LISTEN_BACKLOG 512

class Accpector {
public:
	Accpector(const char* host, int port):host_(host),port_(port) {}

	 st::ErrorPtr init() {
			char sport[8];
			snprintf(sport, sizeof(sport), "%d", port_);
			addrinfo hints;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_NUMERICHOST;
	
			addrinfo* r = NULL;
			if (getaddrinfo(host_.c_str(), sport, (const addrinfo*)&hints, &r)) {
				return MAKE_ERROR(st::ErrorCode::INNER_ERROR, "getaddrinfo error");
			}
	
			int fd = 0;
			if ((fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol)) == -1) {
				LOG(TRACE) << "socket error";
				return MAKE_ERROR(st::ErrorCode::INNER_ERROR,"socket error");
			}
			int v = 1;
			if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &v, sizeof(int)) == -1) {
				LOG(TRACE) << "setsockopt SO_REUSEPORT error";
				return MAKE_ERROR(st::ErrorCode::INNER_ERROR, "setsockopt SO_REUSEPORT error");
			}
	
			v = 1;
			if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int)) == -1) {
				LOG(TRACE) << "setsockopt SO_REUSEADDR error";
				return MAKE_ERROR(st::ErrorCode::INNER_ERROR, "setsockopt SO_REUSEADDR error");
			}
	
			int flags = fcntl(fd, F_GETFD);
			flags |= FD_CLOEXEC;
			if (fcntl(fd, F_SETFD, flags) == -1) {
				LOG(TRACE) << "setsockopt FD_CLOEXEC error";
				return MAKE_ERROR(st::ErrorCode::INNER_ERROR, "setsockopt FD_CLOEXEC error");
			}
	
			if (::bind(fd, r->ai_addr, r->ai_addrlen) == -1) {
				LOG(TRACE) << "bind error";
				return MAKE_ERROR(st::ErrorCode::INNER_ERROR, "bind error");
	
			}
	
			if (::listen(fd, SERVER_LISTEN_BACKLOG) == -1) {
				LOG(TRACE) << "listen error";
				return MAKE_ERROR(st::ErrorCode::INNER_ERROR, "listen error");
	
			}
			if ((listenfd_ = st_netfd_open_socket(fd)) == NULL) {
				LOG(TRACE) << "st_netfd_open_socket error";
				return MAKE_ERROR(st::ErrorCode::INNER_ERROR, "st_netfd_open_socket error");
	
			}
			return NoError;
	}

	 st_netfd_t toAccept(struct sockaddr* addr, int* addrlen) {
		return st_accept(listenfd_, addr, addrlen, ST_UTIME_NO_TIMEOUT);
	 }
private:
	st_netfd_t  listenfd_;
	std::string host_;
	int port_;
};


template<typename ConnPtr>
class SessionManagerImpl {
public:
	void addConn(ConnPtr cli) {
		sesses.push_back(cli);
	}

	void removeConn(ConnPtr cli) {
		LOG(TRACE) << "removeConn";
		stopsesses.push_back(cli);
		sesses.erase(std::find(sesses.begin(), sesses.end(), cli));
	}

	size_t count() const { return sesses.size(); }

	st::ErrorPtr start() {
		co_ = st::coroutine(&::SessionManagerImpl<ConnPtr>::run, this);
		return NoError;
	}

private:
	void run() {
		while (!exit) {
			if (stopsesses.empty())
				st::this_coroutine::sleep_for(std::chrono::seconds(1));
			stopsesses.clear();
		}
	}
private:
	bool exit = false;
	std::vector<ConnPtr> sesses;
	std::vector<ConnPtr> stopsesses;
	st::coroutine co_;
};

class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using SessionManager = SessionManagerImpl<TcpConnectionPtr>;

class TcpConnection:public std::enable_shared_from_this<TcpConnection> {
public:
	TcpConnection(st_netfd_t fd, SessionManager* mgr) :fd_(fd),mgr_(mgr) {
		
	}

	~TcpConnection() {
		exit_ = true;
		st_netfd_close(fd_);
		LOG(TRACE) << "~TcpConnection";
	}

	st::ErrorPtr start() {
		mgr_->addConn(shared_from_this());
		co_ = st::coroutine(&::TcpConnection::run, this);
		return NoError;
	}
private:
	void run() {
		while (!exit_) {
			char buf[1024] = {0};
			int n = st_read(fd_, buf, 1024, ST_UTIME_NO_TIMEOUT);
			if (n == 0) {
				LOG(TRACE) << "peer connection closed!";
				break;
			}else if(n < 0) {
				if (errno == ETIME)
					st::this_coroutine::yield();
				if (errno == EINTR) {
					LOG(TRACE) << "coroutine interrupted!";
					break;
				}
			}else{
				LOG(TRACE) << std::string(buf);
			}
		}
		LOG(TRACE) << "TcpConnection run exit!";
		mgr_->removeConn(shared_from_this());
	}

private:
	bool exit_ = false;
	st_netfd_t fd_;
	SessionManager* mgr_;
	st::coroutine co_;
};


int main() {
	st::enable_coroutine();

	SessionManager mgr;
	mgr.start();

	Accpector acceptor("0.0.0.0",33333);
	auto err = acceptor.init();
	if (err)
		return -1;

	st::coroutine t([&acceptor,&mgr]() {
		while(1){
			auto cli = acceptor.toAccept(NULL, NULL);
			if (cli == nullptr) {
				continue;
			}

			LOG(TRACE) << "after toAccept";
			auto conn = std::make_shared<TcpConnection>(cli,&mgr);
			conn->start();
		}
	});

	LOG(INFO) << "main ";
	
	while (1) {
		LOG(INFO) << "conn count = " << mgr.count();
		st::this_coroutine::sleep_for(std::chrono::seconds(5));
	}
	return 0;
}