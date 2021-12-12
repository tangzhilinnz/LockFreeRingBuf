#ifndef __SINK_BUFFER_H
#define __SINK_BUFFER_H

#include <string>
#include <assert.h>
#include <string.h> // memcpy

#include "Util.h"
#include "StringPiece.h"
#include "Cycles.h"

const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000 * 1000;

template<int SIZE>
class SinkBuffer {
public:
    SinkBuffer(const SinkBuffer&) = delete;
    void operator=(const SinkBuffer&) = delete;

    SinkBuffer()
        : cur_(data_)
        , end_(data_ + sizeof data_)
        , lastRecordedCycles_(0) {
        setCookie(cookieStart);
    }

    ~SinkBuffer() {
        setCookie(cookieEnd);
    }

    void append(const char* /*restrict*/ buf, size_t len) {
        // FIXME: append partially
        //if (implicit_cast<size_t>(avail()) > len) {
            memcpy(cur_, buf, len);
            cur_ += len;
        //}
    }

    void recordCycles() { lastRecordedCycles_ = Cycles::rdtsc(); }
    uint64_t getLastCycles() {
        if (0 == lastRecordedCycles_)
            lastRecordedCycles_ = Cycles::rdtsc();
        return lastRecordedCycles_;
    }
    void resetCycles() { lastRecordedCycles_ = 0; }


    const char* data() const { return data_; }
    int length() const { return static_cast<int>(cur_ - data_); }

    // write to data_ directly
    char* current() { return cur_; }
    int avail() const { return static_cast<int>(end_ - cur_); }
    void add(size_t len) { cur_ += len; }

    void reset() { cur_ = data_; }
    void bzero() { memZero(data_, sizeof data_); }

    // for used by GDB
    const char* debugString() {
        *cur_ = '\0';
        return data_;
    }
    void setCookie(void (*cookie)()) { cookie_ = cookie; }
    // for used by unit test
    std::string toString() const { return std::string(data_, length()); }
    StringPiece toStringPiece() const { return StringPiece(data_, length()); }

private:
    const char* end() const { return end_; }
    // Must be outline function for cookies.
    static void cookieStart() {}
    static void cookieEnd() {}

    void (*cookie_)();
    char data_[SIZE];
    char* cur_;
    char* end_;
    uint64_t lastRecordedCycles_;
};


#endif  // __SINK_BUFFER_H