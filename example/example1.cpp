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

int main() {
	signal(SIGINT, sig_handler);
	LOG(INFO) << "coroutine id:" << st::this_coroutine::get_id();
	st::enable_coroutine();
	std::vector<st::coroutine> vec;
	for (int i = 0; i < 100; i++) {
		vec.emplace_back();
	}
	vec.emplace_back();
	st::coroutine t([]() {
		while (!quit) {
			LOG(INFO) << "coroutine id:" << st::this_coroutine::get_id();
			auto stat = cvar.wait_for(std::chrono::seconds(1));
			switch (stat)
			{
				case st::cv_status::interrupted:
					break;
				case st::cv_status::timeout:
					break;
				case st::cv_status::no_timeout:
					break;
			default:
				break;
			}
		}
	});

	st::coroutine t2([]() {
		int a = 0;
		while (!quit) {
			a++;
			LOG(INFO) << "coroutine id:" << st::this_coroutine::get_id();
			st::this_coroutine::sleep_for(std::chrono::seconds(1));
			if(a == 10)
				cvar.notify_one();
		}
	});

	st::coroutine t3([]() {
		while (!quit) {
			LOG(INFO) << "coroutine id:" << st::this_coroutine::get_id();
			auto stat = cvar.wait_for(std::chrono::seconds(1));
		}
	});

	auto t4 = std::move(st::coroutine([]() {
		while (!quit) {
			LOG(INFO) << "coroutine id:" << st::this_coroutine::get_id();
			auto stat = cvar.wait_for(std::chrono::seconds(1));
		}
		}));
	return 0;
}