#pragma once
#include <st.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string>
#include "coroutine.hpp"
#include "io.hpp"
#include "error.hpp"
#include "autofree.hpp"


namespace st{
	typedef st_netfd_t netfd_t;
	typedef int64_t utime_t;
	class socket;
	typedef socket socket_t;

	#define ST_UTIME_NO_TIMEOUT ((st::utime_t) -1LL)
	#define SERVER_LISTEN_BACKLOG 512

	static void close_stfd(netfd_t& stfd) {
		if (stfd) {
			// we must ensure the close is ok.
			int r0 = st_netfd_close(stfd);
			if (r0) {
				// By _st_epoll_fd_close or _st_kq_fd_close
				if (errno == EBUSY) assert(!r0);
				// By close
				if (errno == EBADF) assert(!r0);
				if (errno == EINTR) assert(!r0);
				if (errno == EIO)   assert(!r0);
				// Others
				assert(!r0);
			}
			stfd = NULL;
		}
	}

	static error_t fd_closeexec(int fd) {
		int flags = fcntl(fd, F_GETFD);
		flags |= FD_CLOEXEC;
		if (fcntl(fd, F_SETFD, flags) == -1) {
			return error_new(ERROR_SOCKET_SETCLOSEEXEC, "FD_CLOEXEC fd=%d", fd);
		}
		return error_ok;
	}

	static error_t fd_reuseaddr(int fd) {
		int v = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(int)) == -1) {
			return error_new(ERROR_SOCKET_SETREUSEADDR, "SO_REUSEADDR fd=%d", fd);
		}
		return error_ok;
	}

	static error_t fd_reuseport(int fd) {
		int v = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &v, sizeof(int)) == -1) {
			LOG(WARNNING) << "SO_REUSEPORT failed for fd=" << fd;
		}
		return error_ok;
	}

	static error_t fd_keepalive(int fd) {
		int v = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &v, sizeof(int)) == -1) {
			return error_new(ERROR_SOCKET_SETKEEPALIVE, "SO_KEEPALIVE fd=%d", fd);
		}
		return error_ok;
	}

	static error_t srs_tcp_connect(std::string server, int port, utime_t tm, netfd_t* pstfd)
	{
		st_utime_t timeout = ST_UTIME_NO_TIMEOUT;
		if (tm != ST_UTIME_NO_TIMEOUT) {
			timeout = tm;
		}

		*pstfd = NULL;
		netfd_t stfd = NULL;

		char sport[8];
		snprintf(sport, sizeof(sport), "%d", port);

		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo* r = NULL;
		SrsAutoFreeH(addrinfo, r, freeaddrinfo);
		if (getaddrinfo(server.c_str(), sport, (const addrinfo*)&hints, &r)) {
			return error_new(ERROR_SYSTEM_IP_INVALID, "get address info");
		}

		int sock = ::socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (sock == -1) {
			return error_new(ERROR_SOCKET_CREATE, "create socket");
		}

		assert(!stfd);
		stfd = st_netfd_open_socket(sock);
		if (stfd == NULL) {
			::close(sock);
			return error_new(ERROR_ST_OPEN_SOCKET, "open socket");
		}

		if (st_connect((st_netfd_t)stfd, r->ai_addr, r->ai_addrlen, timeout) == -1) {
			close_stfd(stfd);
			return error_new(ERROR_ST_CONNECT, "connect to %s:%d", server.c_str(), port);
		}
		*pstfd = stfd;
		return error_ok;
	}

	static error_t do_tcp_listen(int fd, addrinfo* r, netfd_t* pfd)
	{
		error_t err ;
		// Detect alive for TCP connection.
		// @see https://github.com/ossrs/srs/issues/1044
		if ((err = fd_keepalive(fd)) != error_ok) {
			return error_trace(err);
		}

		if ((err = fd_closeexec(fd)) != error_ok) {
			return error_trace(err);
		}

		if ((err = fd_reuseaddr(fd)) != error_ok) {
			return error_trace(err);
		}

		if ((err = fd_reuseport(fd)) != error_ok) {
			return error_trace(err);
		}

		if (::bind(fd, r->ai_addr, r->ai_addrlen) == -1) {
			return error_new(ERROR_SOCKET_BIND, "bind");
		}

		if (::listen(fd, SERVER_LISTEN_BACKLOG) == -1) {
			return error_new(ERROR_SOCKET_LISTEN, "listen");
		}

		if ((*pfd = st_netfd_open_socket(fd)) == NULL) {
			return error_new(ERROR_ST_OPEN_SOCKET, "st open");
		}

		return err;
	}

	static error_t tcp_listen(std::string ip, int port, netfd_t* pfd)
	{
		error_t err;
		char sport[8];
		snprintf(sport, sizeof(sport), "%d", port);

		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_NUMERICHOST;

		addrinfo* r = NULL;
		SrsAutoFreeH(addrinfo, r, freeaddrinfo);
		if (getaddrinfo(ip.c_str(), sport, (const addrinfo*)&hints, &r)) {
			return error_new(ERROR_SYSTEM_IP_INVALID, "getaddrinfo hints=(%d,%d,%d)",hints.ai_family, hints.ai_socktype, hints.ai_flags);
		}

		int fd = 0;
		if ((fd = ::socket(r->ai_family, r->ai_socktype, r->ai_protocol)) == -1) {
			return error_new(ERROR_SOCKET_CREATE, "socket domain=%d, type=%d, protocol=%d",r->ai_family, r->ai_socktype, r->ai_protocol);
		}

		if ((err = do_tcp_listen(fd, r, pfd)) != error_ok) {
			::close(fd);
			return error_trace(err);
		}
		return err;
	}

	static error_t do_udp_listen(int fd, addrinfo* r, netfd_t* pfd)
	{
		error_t err;

		if ((err = fd_closeexec(fd)) != error_ok) {
			return error_trace(err);
		}

		if ((err = fd_reuseaddr(fd)) != error_ok) {
			return error_trace(err);
		}

		if ((err = fd_reuseport(fd)) != error_ok) {
			return error_trace(err);
		}

		if (::bind(fd, r->ai_addr, r->ai_addrlen) == -1) {
			return error_new(ERROR_SOCKET_BIND, "bind");
		}

		if ((*pfd = st_netfd_open_socket(fd)) == NULL) {
			return error_new(ERROR_ST_OPEN_SOCKET, "st open");
		}
		return err;
	}

	static error_t udp_listen(std::string ip, int port, netfd_t* pfd)
	{
		error_t err;

		char sport[8];
		snprintf(sport, sizeof(sport), "%d", port);

		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_NUMERICHOST;

		addrinfo* r = NULL;
		SrsAutoFreeH(addrinfo, r, freeaddrinfo);
		if (getaddrinfo(ip.c_str(), sport, (const addrinfo*)&hints, &r)) {
			return error_new(ERROR_SYSTEM_IP_INVALID, "getaddrinfo hints=(%d,%d,%d)",
				hints.ai_family, hints.ai_socktype, hints.ai_flags);
		}

		int fd = 0;
		if ((fd = ::socket(r->ai_family, r->ai_socktype, r->ai_protocol)) == -1) {
			return error_new(ERROR_SOCKET_CREATE, "socket domain=%d, type=%d, protocol=%d",
				r->ai_family, r->ai_socktype, r->ai_protocol);
		}

		if ((err = do_udp_listen(fd, r, pfd)) != error_ok) {
			::close(fd);
			return  error_trace(err);
		}

		return err;
	}

	class socket
	{
	private:
		// The recv/send timeout in utime_t.
		// @remark Use ST_UTIME_NO_TIMEOUT for never timeout.
		utime_t rtm;
		utime_t stm;
		// The recv/send data in bytes
		int64_t rbytes;
		int64_t sbytes;
		// The underlayer st fd.
		netfd_t stfd;
	public:
		socket();
		virtual ~socket();
	public:
		// Initialize the socket with stfd, user must manage it.
		virtual error_t initialize(netfd_t fd);
	public:
		virtual void set_recv_timeout(utime_t tm);
		virtual utime_t get_recv_timeout();
		virtual void set_send_timeout(utime_t tm);
		virtual utime_t get_send_timeout();
		virtual int64_t get_recv_bytes();
		virtual int64_t get_send_bytes();
	public:
		// @param nread, the actual read bytes, ignore if NULL.
		virtual error_t read(void* buf, size_t size, ssize_t* nread);
		virtual error_t read_fully(void* buf, size_t size, ssize_t* nread);
		// @param nwrite, the actual write bytes, ignore if NULL.
		virtual error_t write(void* buf, size_t size, ssize_t* nwrite);
		virtual error_t writev(const iovec* iov, int iov_size, ssize_t* nwrite);
	};
	
	class TcpServer {
	public:
		TcpServer(const char* host, int port):acceptor_(host,port) {}
		
		error_t start() {
			error_t err;
			err = acceptor_.init();
			if (err) {
				return error_trace(err);
			}
			co_ = st::coroutine(&TcpServer::run,this);
			return error_ok;
		}

		void stop() {
			exit_ = true;
			co_.terminate();
		}
	private:
		void run() {
			while (!exit_) {
				auto nfd = acceptor_.do_accept(NULL,NULL);
				if (nfd == nullptr) {
					continue;
				}
				LOG(INFO) << "accept new client...";
				socket_t sock;
				auto err = sock.initialize(nfd);
				if (err) {
					LOG(ERROR) << err->what();
					continue;
				}
			}
		}
	private:
		class accpector {
		public:
			accpector(const char* host, int port) :host_(host), port_(port) {}

			st::error_t init() {
				return tcp_listen(host_, port_, &listenfd_);
			}

			st::netfd_t do_accept(struct sockaddr* addr, int* addrlen) {
				return st_accept(listenfd_, addr, addrlen, ST_UTIME_NO_TIMEOUT);
			}

		private:
			st::netfd_t  listenfd_;
			std::string host_;
			int port_;
		};
	private:
		accpector acceptor_;
		st::coroutine co_;
		bool exit_ = false;
	};
}