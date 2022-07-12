#pragma once
#include <memory>
#include <string>

namespace st {

enum ErrorCode {
    ERROR_THREAD_STARTED,
    ERROR_THREAD_DISPOSED,
    ERROR_ST_CREATE_CYCLE_THREAD,
    ERROR_THREAD_INTERRUPED, 
    ERROR_THREAD_TERMINATED,
    INNER_ERROR
};

class Error;
using ErrorPtr = std::shared_ptr< st::Error >;
#define NoError std::shared_ptr< st::Error >( nullptr )
class Error {
  public:
    static ErrorPtr make_error( std::string file, int line, ErrorCode code, const std::string& desc = "" ) {
        return std::make_shared< st::Error >( file, line, code, desc );
    }
    Error( std::string file, int line, ErrorCode code, const std::string& desc = "" ) : file_( file ), line_( line ), code_( code ), desc_( desc ) {}

    Error( Error&& err ) {
        this->code_ = err.code_;
        this->desc_ = std::move( err.desc_ );
        this->file_ = std::move( err.file_ );
        this->line_ = err.line_;
    }
    Error& operator=( Error&& err ) {
        this->code_ = err.code_;
        this->desc_ = std::move( err.desc_ );
        this->file_ = std::move( err.file_ );
        this->line_ = err.line_;
        return *this;
    }
    void set_code( ErrorCode code ) {
        code_ = code;
    }
    void set_desc( const std::string& desc ) {
        desc_ = desc;
    }
    std::string file() const {
        return file_;
    }
    int line() const {
        return line_;
    }
    int code() const {
        return code_;
    }
    std::string dest() const {
        return file_ + ":" + std::to_string( line_ );
    }
    const std::string& desc() const {
        return desc_;
    }

  private:
    std::string file_;
    int         line_;
    ErrorCode   code_;
    std::string desc_;
};
#define MAKE_ERROR( code, desc ) st::Error::make_error( __FILE__, __LINE__, code, desc )
}  // namespace st
