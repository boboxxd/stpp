#pragma once
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include "consts.hpp"

#ifndef __FILENAME__
	#define __FILENAME__ (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1):__FILE__)
#endif

namespace st {
	class __error;
	typedef std::shared_ptr<__error> error_t;

	class __error:public std::enable_shared_from_this<__error> {
	public:
		~__error() { stacks_.clear(); }
		__error(const __error&) = delete;
		__error(__error&&) = delete;
		__error& operator=(const __error&) = delete;
		__error& operator=(__error&&) = delete;

		static error_t make_ok() {
			return error_t(nullptr);
		}

		static error_t make_error(const std::string& file, int line, const std::string& fun, int code,const std::string& desc) {
			return error_t(new __error(file, line, fun, code, desc));
		}

		static error_t append(error_t err, const std::string& file, int line, const std::string& fun) {
			return err->append(file,line,fun);
		}

		int code() const {
			return code_;
		}

		std::string desc() const {
			return desc_;
		}
			
		std::string what() const {
			std::stringstream ss;
			ss << "error:" << code_ << " [" << desc_ << "]" << '\n';
			int num = stacks_.size();
			int index = 0;
			for (auto it = stacks_.rbegin(); it!= stacks_.rend();it++,index++) {
				for (int i = 0; i < index; i++) {
					ss << '\t';
				}
				if(index < num-1)
					ss << "--->" << it->file << ":" << it->line << " [" << it->fun << "]" << '\n';
				else
					ss << "--->" << it->file << ":" << it->line << " [" << it->fun << "]" << '\n';
			}
			return ss.str();
		}

	private:
		__error() = default;
		__error(const std::string& file, int line, const std::string& fun,int codec, const std::string& desc) {
			code_ = codec;
			desc_ = desc;
			append(file, line, fun);
		}

		error_t append(const std::string& file, int line, const std::string& fun) {
			stacks_.emplace_back(file,line,fun);
			return shared_from_this();
		}
	private:
		struct __erroritem {
			std::string file;
			int         line;
			std::string fun;
			__erroritem(const std::string& filev, int linev, const std::string& funv):file(filev),line(linev),fun(funv) {}
		};

		int code_;
		std::string desc_;
		std::vector<__erroritem> stacks_;
	};

}

#define error_new(code,desc) st::__error::make_error(__FILENAME__, __LINE__, __FUNCTION__, code,desc) 
#define error_trace(err) st::__error::append(err,__FILENAME__, __LINE__, __FUNCTION__)
#define error_ok st::__error::make_ok() 