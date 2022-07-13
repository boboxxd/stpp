#pragma once
#include <st.h>
#include <sys/uio.h>

namespace st {

     /**
     * The system io reader/writer architecture:
     *                                         +---------------+  +---------------+
     *                                         | IStreamWriter |  | IVectorWriter |
     *                                         +---------------+  +---------------+
     *                                         | + write()     |  | + writev()    |
     *                                         +-------------+-+  ++--------------+
     * +----------+     +--------------------+               /\   /\
     * | IReader  |     |    IStatistic      |                 \ /
     * +----------+     +--------------------+                  V
     * | + read() |     | + get_recv_bytes() |           +------+----+
     * +------+---+     | + get_send_bytes() |           |  IWriter  |
     *       / \        +---+--------------+-+           +-------+---+
     *        |            / \            / \                   / \
     *        |             |              |                     |
     * +------+-------------+------+      ++---------------------+--+
     * | IProtocolReader           |      | IProtocolWriter         |
     * +---------------------------+      +-------------------------+
     * | + readfully()             |      | + set_send_timeout()    |
     * | + set_recv_timeout()      |      +-------+-----------------+
     * +------------+--------------+             / \
     *             / \                            |
     *              |                             |
     *           +--+-----------------------------+-+
     *           |       IProtocolReadWriter        |
     *           +----------------------------------+
     */

  /**
  * The reader to read data from channel.
  */
    class ISrsReader
    {
    public:
        ISrsReader();
        virtual ~ISrsReader();
    public:
        /**
         * Read bytes from reader.
         * @param nread How many bytes read from channel. NULL to ignore.
         */
        virtual srs_error_t read(void* buf, size_t size, ssize_t* nread) = 0;
    };

    /**
     * The seeker to seek with a device.
     */
    class ISrsSeeker
    {
    public:
        ISrsSeeker();
        virtual ~ISrsSeeker();
    public:
        /**
         * The lseek() function repositions the offset of the file descriptor fildes to the argument offset, according to the
         * directive whence. lseek() repositions the file pointer fildes as follows:
         *      If whence is SEEK_SET, the offset is set to offset bytes.
         *      If whence is SEEK_CUR, the offset is set to its current location plus offset bytes.
         *      If whence is SEEK_END, the offset is set to the size of the file plus offset bytes.
         * @param seeked Upon successful completion, lseek() returns the resulting offset location as measured in bytes from
         *      the beginning of the file. NULL to ignore.
         */
        virtual srs_error_t lseek(off_t offset, int whence, off_t* seeked) = 0;
    };

    /**
     * The reader and seeker.
     */
    class ISrsReadSeeker : public ISrsReader, public ISrsSeeker
    {
    public:
        ISrsReadSeeker();
        virtual ~ISrsReadSeeker();
    };

    /**
     * The writer to write stream data to channel.
     */
    class ISrsStreamWriter
    {
    public:
        ISrsStreamWriter();
        virtual ~ISrsStreamWriter();
    public:
        /**
         * write bytes over writer.
         * @nwrite the actual written bytes. NULL to ignore.
         */
        virtual srs_error_t write(void* buf, size_t size, ssize_t* nwrite) = 0;
    };

    /**
     * The vector writer to write vector(iovc) to channel.
     */
    class ISrsVectorWriter
    {
    public:
        ISrsVectorWriter();
        virtual ~ISrsVectorWriter();
    public:
        /**
         * write iov over writer.
         * @nwrite the actual written bytes. NULL to ignore.
         * @remark for the HTTP FLV, to writev to improve performance.
         *      @see https://github.com/ossrs/srs/issues/405
         */
        virtual srs_error_t writev(const iovec* iov, int iov_size, ssize_t* nwrite) = 0;
    };

    /**
     * The generally writer, stream and vector writer.
     */
    class ISrsWriter : public ISrsStreamWriter, public ISrsVectorWriter
    {
    public:
        ISrsWriter();
        virtual ~ISrsWriter();
    };

    /**
     * The writer and seeker.
     */
    class ISrsWriteSeeker : public ISrsWriter, public ISrsSeeker
    {
    public:
        ISrsWriteSeeker();
        virtual ~ISrsWriteSeeker();
    };

    /**
     * Get the statistic of channel.
     */
    class ISrsProtocolStatistic
    {
    public:
        ISrsProtocolStatistic();
        virtual ~ISrsProtocolStatistic();
        // For protocol
    public:
        // Get the total recv bytes over underlay fd.
        virtual int64_t get_recv_bytes() = 0;
        // Get the total send bytes over underlay fd.
        virtual int64_t get_send_bytes() = 0;
    };

    /**
     * the reader for the protocol to read from whatever channel.
     */
    class ISrsProtocolReader : public ISrsReader, virtual public ISrsProtocolStatistic
    {
    public:
        ISrsProtocolReader();
        virtual ~ISrsProtocolReader();
        // for protocol
    public:
        // Set the timeout tm in srs_utime_t for recv bytes from peer.
        // @remark Use SRS_UTIME_NO_TIMEOUT to never timeout.
        virtual void set_recv_timeout(srs_utime_t tm) = 0;
        // Get the timeout in srs_utime_t for recv bytes from peer.
        virtual srs_utime_t get_recv_timeout() = 0;
        // For handshake.
    public:
        // Read specified size bytes of data
        // @param nread, the actually read size, NULL to ignore.
        virtual srs_error_t read_fully(void* buf, size_t size, ssize_t* nread) = 0;
    };

    /**
     * the writer for the protocol to write to whatever channel.
     */
    class ISrsProtocolWriter : public ISrsWriter, virtual public ISrsProtocolStatistic
    {
    public:
        ISrsProtocolWriter();
        virtual ~ISrsProtocolWriter();
        // For protocol
    public:
        // Set the timeout tm in srs_utime_t for send bytes to peer.
        // @remark Use SRS_UTIME_NO_TIMEOUT to never timeout.
        virtual void set_send_timeout(srs_utime_t tm) = 0;
        // Get the timeout in srs_utime_t for send bytes to peer.
        virtual srs_utime_t get_send_timeout() = 0;
    };

    /**
     * The reader and writer.
     */
    class ISrsProtocolReadWriter : public ISrsProtocolReader, public ISrsProtocolWriter
    {
    public:
        ISrsProtocolReadWriter();
        virtual ~ISrsProtocolReadWriter();
    };

}