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

namespace st {
	typedef st_netfd_t netfd_t;
	typedef int64_t utime_t;

#define UTIME_NO_TIMEOUT ((st::utime_t) -1LL)
#define SERVER_LISTEN_BACKLOG 512
	// The time unit in ms, for example 100 * SRS_UTIME_MILLISECONDS means 100ms.
#define UTIME_MILLISECONDS 1000

// Convert srs_utime_t as ms.
#define u2ms(us) ((us) / UTIME_MILLISECONDS)
#define u2msi(us) int((us) / UTIME_MILLISECONDS)

// Them time duration = end - start. return 0, if start or end is 0.
	utime_t srs_duration(utime_t start, utime_t end);

	// The time unit in ms, for example 120 * SRS_UTIME_SECONDS means 120s.
#define UTIME_SECONDS 1000000LL

// The time unit in minutes, for example 3 * SRS_UTIME_MINUTES means 3m.
#define UTIME_MINUTES 60000000LL

// The time unit in hours, for example 2 * SRS_UTIME_HOURS means 2h.
#define UTIME_HOURS 3600000000LL

	namespace __detail {
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
			utime_t timeout = UTIME_NO_TIMEOUT;
			if (tm != UTIME_NO_TIMEOUT) {
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
			error_t err;
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
				return error_new(ERROR_SYSTEM_IP_INVALID, "getaddrinfo hints=(%d,%d,%d)", hints.ai_family, hints.ai_socktype, hints.ai_flags);
			}

			int fd = 0;
			if ((fd = ::socket(r->ai_family, r->ai_socktype, r->ai_protocol)) == -1) {
				return error_new(ERROR_SOCKET_CREATE, "socket domain=%d, type=%d, protocol=%d", r->ai_family, r->ai_socktype, r->ai_protocol);
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

		class accpector {
		public:
			accpector(const char* host, int port) :host_(host), port_(port) {}

			st::error_t init() {
				return tcp_listen(host_, port_, &listenfd_);
			}

			st::netfd_t do_accept(struct sockaddr* addr, int* addrlen) {
				return st_accept(listenfd_, addr, addrlen, UTIME_NO_TIMEOUT);
			}

		private:
			st::netfd_t  listenfd_;
			std::string host_;
			int port_;
		};
	}

	class Socket
	{
	private:
		// The recv/send timeout in utime_t.
		// @remark Use UTIME_NO_TIMEOUT for never timeout.
		utime_t rtm;
		utime_t stm;
		// The recv/send data in bytes
		int64_t rbytes;
		int64_t sbytes;
		netfd_t stfd;
	public:
		Socket() {
			stfd = NULL;
			stm = rtm = UTIME_NO_TIMEOUT;
			rbytes = sbytes = 0;
		}

		virtual ~Socket() {
			if (stfd) {
				__detail::close_stfd(stfd);
			}
		}

	public:
		// Initialize the socket with stfd, user must manage it.
		virtual error_t initialize(netfd_t fd) { stfd = fd; return error_ok; }
	public:
		virtual void set_recv_timeout(utime_t tm) { rtm = tm; }
		virtual utime_t get_recv_timeout() { return rtm; }
		virtual void set_send_timeout(utime_t tm) { stm = tm; }
		virtual utime_t get_send_timeout() { return stm; }
		virtual int64_t get_recv_bytes() { return rbytes; }
		virtual int64_t get_send_bytes() { return sbytes; }
	public:
		// @param nread, the actual read bytes, ignore if NULL.
		virtual error_t read(void* buf, size_t size, ssize_t* nread) {
			error_t err;

			ssize_t nb_read;
			if (rtm == UTIME_NO_TIMEOUT) {
				nb_read = st_read((st_netfd_t)stfd, buf, size, UTIME_NO_TIMEOUT);
			}
			else {
				nb_read = st_read((st_netfd_t)stfd, buf, size, rtm);
			}

			if (nread) {
				*nread = nb_read;
			}

			// On success a non-negative integer indicating the number of bytes actually read is returned
			// (a value of 0 means the network connection is closed or end of file is reached).
			// Otherwise, a value of -1 is returned and errno is set to indicate the error.
			if (nb_read <= 0) {
				if (nb_read < 0 && errno == ETIME) {
					return error_new(ERROR_SOCKET_TIMEOUT, "timeout %d ms", u2msi(rtm));
				}

				if (nb_read == 0) {
					errno = ECONNRESET;
				}

				return error_new(ERROR_SOCKET_READ, "read");
			}

			rbytes += nb_read;

			return err;
		}

		virtual error_t read_fully(void* buf, size_t size, ssize_t* nread) {
			error_t err;

			ssize_t nb_read;
			if (rtm == UTIME_NO_TIMEOUT) {
				nb_read = st_read_fully((st_netfd_t)stfd, buf, size, UTIME_NO_TIMEOUT);
			}
			else {
				nb_read = st_read_fully((st_netfd_t)stfd, buf, size, rtm);
			}

			if (nread) {
				*nread = nb_read;
			}

			// On success a non-negative integer indicating the number of bytes actually read is returned
			// (a value less than nbyte means the network connection is closed or end of file is reached)
			// Otherwise, a value of -1 is returned and errno is set to indicate the error.
			if (nb_read != (ssize_t)size) {
				if (nb_read < 0 && errno == ETIME) {
					return error_new(ERROR_SOCKET_TIMEOUT, "timeout %d ms", u2msi(rtm));
				}

				if (nb_read >= 0) {
					errno = ECONNRESET;
				}

				return error_new(ERROR_SOCKET_READ_FULLY, "read fully");
			}

			rbytes += nb_read;

			return err;
		}

		// @param nwrite, the actual write bytes, ignore if NULL.
		virtual error_t write(void* buf, size_t size, ssize_t* nwrite) {
			error_t err;

			ssize_t nb_write;
			if (stm == UTIME_NO_TIMEOUT) {
				nb_write = st_write((st_netfd_t)stfd, buf, size, UTIME_NO_TIMEOUT);
			}
			else {
				nb_write = st_write((st_netfd_t)stfd, buf, size, stm);
			}

			if (nwrite) {
				*nwrite = nb_write;
			}

			// On success a non-negative integer equal to nbyte is returned.
			// Otherwise, a value of -1 is returned and errno is set to indicate the error.
			if (nb_write <= 0) {
				if (nb_write < 0 && errno == ETIME) {
					return error_new(ERROR_SOCKET_TIMEOUT, "write timeout %d ms", u2msi(stm));
				}

				return error_new(ERROR_SOCKET_WRITE, "write");
			}

			sbytes += nb_write;

			return err;
		}

		virtual error_t writev(const iovec* iov, int iov_size, ssize_t* nwrite) {
			error_t err;

			ssize_t nb_write;
			if (stm == UTIME_NO_TIMEOUT) {
				nb_write = st_writev((st_netfd_t)stfd, iov, iov_size, ST_UTIME_NO_TIMEOUT);
			}
			else {
				nb_write = st_writev((st_netfd_t)stfd, iov, iov_size, stm);
			}

			if (nwrite) {
				*nwrite = nb_write;
			}

			// On success a non-negative integer equal to nbyte is returned.
			// Otherwise, a value of -1 is returned and errno is set to indicate the error.
			if (nb_write <= 0) {
				if (nb_write < 0 && errno == ETIME) {
					return error_new(ERROR_SOCKET_TIMEOUT, "writev timeout %d ms", u2msi(stm));
				}

				return error_new(ERROR_SOCKET_WRITE, "writev");
			}

			sbytes += nb_write;

			return err;
		}
	};

	using SocketPtr = std::shared_ptr<Socket>;
	using CodecCallback = std::function<void(std::vector<unsigned char>&&)>;
	template<typename Server>
	class TcpConnection;
	class TcpServer;
	using TcpConnectionPtr = std::shared_ptr<TcpConnection<TcpServer>>;
	using TcpConnectionHandler = std::function<void(TcpConnectionPtr conn)>;

	class IProtoCodec {
	public:
		virtual ~IProtoCodec() {}
		virtual error_t encode(unsigned char* data, size_t len, CodecCallback cbk) = 0;
		virtual error_t decode(unsigned char* data, size_t len, st::CodecCallback cbk) = 0;
	};
	template<typename Server>
	class TcpConnection :public std::enable_shared_from_this<TcpConnection<Server>> {
	public:
		TcpConnection(SocketPtr sock, IProtoCodec* codec, Server* svr) :sock_(sock), codec_(codec), svr_(svr) {}

		~TcpConnection() {
		}

		error_t read(std::vector<unsigned char>& data) {
			error_t err;
			ssize_t nread = 0;
			char buf[4096];
			err = sock_->read(buf, 4096, &nread);
			err = codec_->decode((unsigned char*)buf, nread, [&](std::vector<unsigned char>&& v) {
				data = std::move(v);
				});
			return err;
		}

		error_t write(void* buf, size_t size) {
			error_t err;
			err = codec_->encode((unsigned char*)buf, size, [&](std::vector<unsigned char>&& v) {
				ssize_t nwrite = 0;
				err = sock_->write(v.data(), v.size(), &nwrite);
				});
			return err;
		}

		void onNewConnection(TcpConnectionHandler handler) {
			//LOG(TRACE) << "accept new client...";
			co_ = st::coroutine([this](TcpConnectionHandler handler) {
				handler(this->shared_from_this());
				svr_->removeConnecttion(this->shared_from_this()); }, handler);
		}

	protected:
		SocketPtr sock_;
		st::coroutine co_;
		IProtoCodec* codec_;
		Server* svr_;
	};

	class TcpServer {
	public:
		friend TcpConnection<TcpServer>;
		TcpServer(const char* host, int port) :acceptor_(host, port) {}

		error_t start() {
			error_t err;
			err = acceptor_.init();
			if (err) {
				return error_trace(err);
			}
			co_ = st::coroutine(&TcpServer::run, this);
			idolco_ = st::coroutine(&TcpServer::idol, this);
			return error_ok;
		}

		void stop() {
			exit_ = true;
			clear_cond_.notify_all();
			co_.terminate();
		}

		void onNewConnection(IProtoCodec* codec, TcpConnectionHandler handler) {
			codec_ = codec;
			handler_ = std::move(handler);
		}

	private:
		void run() {
			while (!exit_) {
				auto nfd = acceptor_.do_accept(NULL, NULL);
				if (nfd == nullptr) {
					continue;
				}

				auto sock = SocketPtr(new Socket());
				auto err = sock->initialize(nfd);
				if (err) {
					LOG(ERROR) << err->what();
					continue;
				}
				addConnection(TcpConnectionPtr(new TcpConnection<TcpServer>(sock, codec_, this)));
			}
		}

		//clear connection
		void idol() {
			while (!exit_) {
				clear_cond_.wait();
				//LOG(TRACE) << "release connection num:" << closed_cliconns_.size();
				closed_cliconns_.clear();
			}
		}
	private:
		void removeConnecttion(TcpConnectionPtr conn) {
			closed_cliconns_.push_back(conn);
			alive_cliconns_.erase(std::find(alive_cliconns_.begin(), alive_cliconns_.end(), conn));
			clear_cond_.notify_one();
		}

		void addConnection(TcpConnectionPtr conn) {
			conn->onNewConnection(handler_);
			alive_cliconns_.push_back(conn);
		}

	private:
		__detail::accpector acceptor_;
		st::coroutine co_;					 //acceptЭ��
		st::coroutine idolco_;
		bool exit_ = false;
		IProtoCodec* codec_;
		TcpConnectionHandler handler_;
		std::vector<TcpConnectionPtr> alive_cliconns_; //client co
		std::vector<TcpConnectionPtr> closed_cliconns_;
		st::condition_variable clear_cond_;
	};
}