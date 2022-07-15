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

st::condition_variable cvar;

bool quit = false;

void sig_handler(int sigNo) {
	quit = true;
	cvar.notify_all();
}

namespace Test {
	static const int num = 10000;

	void testThread() {
		std::vector<std::thread> v;

		for (int i = 0; i < num; i++) {
			v.emplace_back([]() {
				for (;;) {
					LOG(INFO) << "thread";
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
				});
		}
		for (auto& it : v) {
			it.join();
		}
	}

	void testCoroutine() {
		std::vector<st::coroutine> v;

		for (int i = 0; i < num; i++) {
			v.emplace_back(1, []() {
				for (;;) {
					LOG(INFO) << "coroutine";
					st::this_coroutine::sleep_for(std::chrono::milliseconds(10));
				}
				});
		}
	}
}

int main(int argc, char** argv) {
	signal(SIGINT, sig_handler);
	LOG(INFO) << "coroutine id:" << st::this_coroutine::get_id();
	st::enable_coroutine();;
	if (argc == 1)
		Test::testThread();
	else
		Test::testCoroutine();
	return 0;
}