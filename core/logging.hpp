#pragma once
#include <chrono>
#include <iomanip>
#include <iostream>
#include <cstring>
#include "coroutine.hpp"
#define __FILENAME__ (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1):__FILE__)
enum {TRACE,INFO,WARNNING,ERROR};

namespace st{
    namespace this_coroutine {
        long get_id();
    }

    static std::string GetCurrentTimeStamp()
    {
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

        std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm* now_tm = std::localtime(&now_time_t);

        char buffer[128];
        strftime(buffer, sizeof(buffer), "%F %T", now_tm);

        std::ostringstream ss;
        ss.fill('0');
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()) % 1000000000;
        ss << buffer << "." <<std::setw(8)<<std::setfill('0')<<setiosflags(std::ios::right)<<ns.count();
        return ss.str();
    }


    class LogStream{
        public:
            LogStream(std::ostream& stream,int level ,const char* file,int line):os(stream){
                std::string lestr;
                switch(level){
                    case TRACE:
                        lestr = "TRACE";
                    break;
                    case INFO:
                        lestr ="INFO";
                    break;
                    case WARNNING:
                        lestr ="WARNNING";
                    break;
                    case ERROR:
                        lestr ="ERROR";
                    break;
                }
                os <<lestr<<" "<<GetCurrentTimeStamp()<<"|"<<st::this_coroutine::get_id()<<"|"<< file<<":"<<line<<"|> ";
            }
            ~LogStream(){
                os <<'\n';
            }
            std::ostream& os;
    };
}

#define LOG(level) st::LogStream(std::cout,level,__FILENAME__,__LINE__).os
