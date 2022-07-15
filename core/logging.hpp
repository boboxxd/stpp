#pragma once
#include <chrono>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <sstream>
#include <thread>
#define ENABLE_ST_COROUTINE

#ifdef ENABLE_ST_COROUTINE
#include "coroutine.hpp"
namespace st {
	namespace this_coroutine {
		long get_id();
	}
}
#endif

#define __FILENAME__ (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1):__FILE__)
enum { TRACE, INFO, WARNNING, ERROR };

namespace st {
	static std::string GetCurrentTimeStamp()
	{
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

		std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
		std::tm* now_tm = std::localtime(&now_time_t);

		char buffer[128];
		//Date:\n%Y-%m-%d\nTime:\n%I:%M:%S\n"
		strftime(buffer, sizeof(buffer), "%Y%m%d %H:%M:%S", now_tm);

		std::stringstream ss;
		ss.fill('0');
		auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()) % 1000000000;
		ss << buffer << "." << std::setw(8) << std::setfill('0') << std::setiosflags(std::ios::right) << ns.count();
		return ss.str();
	}

	class LogStream {
	public:
		LogStream(std::ostream& stream, int level, const char* file, int line) :os(stream), _level(level) {
			if (level < __log_level_limit)
				return;
			std::string lestr;
			switch (level) {
			case TRACE:
				lestr = "T";
				break;
			case INFO:
				lestr = "I";
				break;
			case WARNNING:
				lestr = "W";
				break;
			case ERROR:
				lestr = "E";
				break;
			}

#ifdef ENABLE_ST_COROUTINE
			ss << lestr << GetCurrentTimeStamp() << " " << std::this_thread::get_id() << " " << st::this_coroutine::get_id() << " " << file << ":" << line << "] ";
#else
			ss << lestr << GetCurrentTimeStamp() << " " << std::this_thread::get_id() << " " << file << ":" << line << "] ";
#endif
		}

		~LogStream() {
			if (_level < __log_level_limit)
				return;
			ss << '\n';
			os << ss.str();
		}

		static void setLogLevel(int level) { LogStream::__log_level_limit = level; }

		std::ostream& os;
		std::stringstream ss;
	private:
		static int __log_level_limit;
		int _level;
		};

	int LogStream::__log_level_limit = INFO;
	}

#define LOG(level) st::LogStream(std::cout,level,__FILENAME__,__LINE__).ss