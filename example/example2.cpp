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

int main() {
	signal(SIGINT, sig_handler);
	st::enable_coroutine();
	st::LogStream::setLogLevel(TRACE);

	st::TcpServer svr("0.0.0.0", 33332);

	svr.setConnectHandler([](st::SocketPtr sock) {
		LOG(INFO) << "setConnectHandler";
		char buf[4096];
		ssize_t readn = 0;
		ssize_t writen = 0;
		auto err = sock->read(buf,4096,&readn);
		if (err) {
			LOG(ERROR) << err->what();
		}
		else {
			LOG(INFO) << "has read:" << sock->get_recv_bytes() << " bytes.";
			LOG(INFO) << "read data:" << std::string(buf);
			err = sock->write((void*)"yes", 4, &writen);
			if (err) {
				LOG(ERROR) << err->what();
			}
			else {
				LOG(INFO) << "has write:" << sock->get_send_bytes()<<" bytes.";
			}
		}
	});

	auto err = svr.start();
	if (err) {
		LOG(ERROR) << err->what();
		return -1;
	}

	stopcon.wait();
	svr.stop();
	LOG(INFO) << "program exit normally ...";
	return 0;
}