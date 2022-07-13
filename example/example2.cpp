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
#include <arpa/inet.h>
#include <vector>
#include "core/error.hpp"
#include "core/net.hpp"
class Accpector {
public:
	Accpector(const char* host, int port):host_(host),port_(port) {}

	 st::error_t init() {
			char sport[8];
			snprintf(sport, sizeof(sport), "%d", port_);
			addrinfo hints;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_NUMERICHOST;
	
			addrinfo* r = NULL;
			if (getaddrinfo(host_.c_str(), sport, (const addrinfo*)&hints, &r)) {
				return error_new(ERROR_INNER, "getaddrinfo error");
			}
	
			int fd = 0;
			if ((fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol)) == -1) {
				return error_new(ERROR_INNER,"socket error");
			}
			int v = 1;
			if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &v, sizeof(int)) == -1) {
				return error_new(ERROR_INNER, "setsockopt SO_REUSEPORT error");
			}
	
			v = 1;
			if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int)) == -1) {
				return error_new(ERROR_INNER, "setsockopt SO_REUSEADDR error");
			}
	
			int flags = fcntl(fd, F_GETFD);
			flags |= FD_CLOEXEC;
			if (fcntl(fd, F_SETFD, flags) == -1) {
				return error_new(ERROR_INNER, "setsockopt FD_CLOEXEC error");
			}
	
			if (::bind(fd, r->ai_addr, r->ai_addrlen) == -1) {
				return error_new(ERROR_INNER, "bind error");
	
			}
	
			if (::listen(fd, SERVER_LISTEN_BACKLOG) == -1) {
				return error_new(ERROR_INNER, "listen error");
	
			}
			if ((listenfd_ = st_netfd_open_socket(fd)) == NULL) {
				return error_new(ERROR_INNER, "st_netfd_open_socket error");
	
			}
			return error_ok;
	}

	 st::netfd_t toAccept(struct sockaddr* addr, int* addrlen) {
		return st_accept(listenfd_, addr, addrlen, ST_UTIME_NO_TIMEOUT);
	 }

private:
	st::netfd_t  listenfd_;
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

	st::error_t start() {
		co_ = st::coroutine(&::SessionManagerImpl<ConnPtr>::run, this);
		return error_ok;
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
	TcpConnection(st::netfd_t fd, SessionManager* mgr) :fd_(fd),mgr_(mgr) {
		
	}

	~TcpConnection() {
		exit_ = true;
		st_netfd_close(fd_);
		LOG(TRACE) << "~TcpConnection";
	}

	st::error_t start() {
		mgr_->addConn(shared_from_this());
		co_ = st::coroutine(&::TcpConnection::run, this);
		return error_ok;
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
	st::netfd_t fd_;
	SessionManager* mgr_;
	st::coroutine co_;
};


int main() {
	st::enable_coroutine();
	st::LogStream::setLogLevel(TRACE);

	//st::error_t e = error_new(ERROR_INNER, "this is an error!");
	
	SessionManager mgr;
	mgr.start();

	Accpector acceptor("0.0.0.0",33333);
	auto err = acceptor.init();
	if (err && err->code() != ERROR_OK) {
		LOG(ERROR) << err->what();
		return -1;
	}
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

	std::thread t2([]() {
	LOG(INFO) << "t2 = ";
	});
	t2.join();
	while (1) {
		LOG(INFO) << "conn count = " << mgr.count();
		st::this_coroutine::sleep_for(std::chrono::seconds(5));
	}
	return 0;
}