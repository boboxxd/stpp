#pragma once
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <cstdarg>
#include "consts.hpp"


#ifndef __FILENAME__
	#define __FILENAME__ (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1):__FILE__)
#endif

namespace st {
	class __error;
	typedef std::shared_ptr<__error> error_t;

	class __error{
	public:
		~__error() { stacks_.clear(); }
		__error(const __error&) = delete;
		__error(__error&&) = delete;
		__error& operator=(const __error&) = delete;
		__error& operator=(__error&&) = delete;

		static error_t make_ok() {
			return error_t(nullptr);
		}
		
		static error_t make_error(const std::string& file, int line, const std::string& fun, int code, const char* fmt, ...) {
			va_list ap;
			va_start(ap, fmt);
			static char buffer[4096];
			vsnprintf(buffer, sizeof(buffer), fmt, ap);
			va_end(ap);
			return error_t(new __error(file, line, fun, code, std::string(buffer)));
		}

		static error_t append(error_t err, const std::string& file, int line, const std::string& fun) {
			err->append(file,line,fun);
			return err;
		}

		int code() const {
			return code_;
		}

		std::string desc() const {
			return desc_;
		}
			
		std::string what() const {
			std::stringstream ss;
			ss << "error no:" << code_ << ",desc:" << desc_  << '\n';
			int num = stacks_.size();
			int index = 0;
			for (auto it = stacks_.rbegin(); it!= stacks_.rend();it++,index++) {
				for (int i = 0; i < index; i++) {
					ss << "  ";
				}
				ss << "->" << it->file << ":" << it->line << " [" << it->fun << "]";
				if (index < num - 1)
					ss << '\n';
			}
			return ss.str();
		}

	private:
		__error() = default;
		__error(const std::string& file, int line, const std::string& fun,int codec, const std::string& desc) {
			rerrno_ = (int)errno;
			code_ = codec;
			desc_ = desc;
			append(file, line, fun);
		}

		void append(const std::string& file, int line, const std::string& fun) {
			stacks_.emplace_back(file,line,fun);
		}
	private:
		struct __erroritem {
			std::string file;
			int         line;
			std::string fun;
			__erroritem(const std::string& filev, int linev, const std::string& funv):file(filev),line(linev),fun(funv) {}
		};

		int code_;
		int rerrno_;
		std::string desc_;
		std::vector<__erroritem> stacks_;
	};

}

#define error_new(code, fmt, ...) st::__error::make_error(__FILENAME__, __LINE__, __FUNCTION__, code, fmt, ##__VA_ARGS__)
#define error_trace(err) st::__error::append(err,__FILENAME__, __LINE__, __FUNCTION__)
#define error_ok st::__error::make_ok() 