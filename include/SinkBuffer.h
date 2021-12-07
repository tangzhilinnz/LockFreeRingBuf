#ifndef __SINK_BUFFER_H
#define __SINK_BUFFER_H

#include <string>
#include <assert.h>
#include <string.h> // memcpy

#include "Util.h"
#include "StringPiece.h"

const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000 * 1000;

template<int SIZE>
class SinkBuffer {
public:
    SinkBuffer(const SinkBuffer&) = delete;
    void operator=(const SinkBuffer&) = delete;

    SinkBuffer()
        : cur_(data_) {
        setCookie(cookieStart);
    }

    ~SinkBuffer() {
        setCookie(cookieEnd);
    }

    void append(const char* /*restrict*/ buf, size_t len) {
        // FIXME: append partially
        if (implicit_cast<size_t>(avail()) > len) {
            memcpy(cur_, buf, len);
            cur_ += len;
        }
    }

    const char* data() const { return data_; }
    int length() const { return static_cast<int>(cur_ - data_); }

    // write to data_ directly
    char* current() { return cur_; }
    int avail() const { return static_cast<int>(end() - cur_); }
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
    const char* end() const { return data_ + sizeof data_; }
    // Must be outline function for cookies.
    static void cookieStart() {}
    static void cookieEnd() {}

    void (*cookie_)();
    char data_[SIZE];
    char* cur_;
};


#endif  // __SINK_BUFFER_H