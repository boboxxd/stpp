#include <signal.h>
#include <vector>
#include <ostream>
#include <sstream>
#include <iostream>
#include <signal.h>
#include "core/stpp.h"

st::condition_variable  stopcon;

void sig_handler(int signo) {
	stopcon.notify_all();
}

class MyCodec :public st::IProtoCodec {
public:
	virtual st::error_t encode(unsigned char* data, size_t len, st::CodecCallback cbk) override {
		std::vector<unsigned char> out(data, data + len);
		cbk(std::move(out));
		//LOG(INFO) << "encoded";
		return error_ok;
	}

	virtual st::error_t decode(unsigned char* data, size_t len, st::CodecCallback cbk) override {
		std::vector<unsigned char> out(data, data + len);
		cbk(std::move(out));
		//LOG(INFO) << "decode";
		return error_ok;
	}
};

int main() {
	signal(SIGINT, sig_handler);
	st::enable_coroutine();
	st::LogStream::setLogLevel(TRACE);
	int port = 33332;
	st::TcpServer svr("0.0.0.0", port);
	auto err = svr.start();
	if (err) {
		LOG(ERROR) << err->what();
		return -1;
	}
	LOG(INFO) << "server listen on: " << port;

	svr.onNewConnection(new MyCodec(), [](st::TcpConnectionPtr conn) {
		LOG(INFO) << "accept new client...";
		std::vector<unsigned char> recvbuf;
		conn->read(recvbuf);
		auto tt = st::GetCurrentTimeStamp();
		LOG(INFO) << tt;
		std::stringstream ss;
		ss << "HTTP/1.1 200 OK" << "\r\n";
		ss << "Content-Type: text/plain" << "\r\n";
		ss << "Content-Length: " << tt.size() << "\r\n";
		ss << "\r\n";
		ss << tt;
		conn->write((void*)ss.str().data(), ss.str().size());
		});

	stopcon.wait();

	svr.stop();
	LOG(INFO) << "program exit normally ...";
	return 0;
}