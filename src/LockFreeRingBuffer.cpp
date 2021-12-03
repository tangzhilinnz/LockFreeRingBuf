#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <mutex>
#include <stdlib.h>
#include <memory>
#include <array>

#include "active.h"
#include "RingQueue.h"

// #include "mimalloc-new-delete.h"
// #include "mimalloc-override.h"
// #include "mimalloc.h"

using namespace std;
using namespace chrono;


//#define USE_ACTIVE

template <size_t ArgBytes>
struct UncompressedEntry {
    // Uniquely identifies a log message by its format string and file
    // location, assigned at compile time by the preprocessor.
    uint32_t fmtId;

    // Number of bytes for this header and the various uncompressed
    // log arguments after it
    uint32_t entrySize;

    // Stores the rdtsc() value at the time of the log function invocation
    uint64_t timestamp;

    // After this header are the uncompressed arguments required by
    // the original format string
    //char argData[48];
    std::array<char, ArgBytes> argData;

    mutable  std::unique_ptr<char[]> dyMem;

    UncompressedEntry(int numBytesDym)
        : dyMem(new char[numBytesDym]) {
    }

    UncompressedEntry() {}

    // UncompressedEntry(const UncompressedEntry& other) = delete;
    // UncompressedEntry& operator=(const UncompressedEntry& other) = delete;

    UncompressedEntry(const UncompressedEntry& other)
        : fmtId(other.fmtId)
        , entrySize(other.entrySize)
        , timestamp(other.timestamp)
        , dyMem(other.dyMem.release()) {

        memcpy(argData.data(), other.argData.data(), ArgBytes);
    }

    UncompressedEntry(UncompressedEntry&& other)
        : fmtId(other.fmtId)
        , entrySize(other.entrySize)
        , timestamp(other.timestamp)
        , dyMem(other.dyMem.release()) {

        memcpy(argData.data(), other.argData.data(), ArgBytes);
    }

    UncompressedEntry& operator=(const UncompressedEntry& other) {
        fmtId = other.fmtId;
        entrySize = other.entrySize;
        timestamp = other.timestamp;
        memcpy(argData.data(), other.argData.data(), ArgBytes);
        dyMem.reset(other.dyMem.release());

        return *this;
    }

    UncompressedEntry& operator=(UncompressedEntry&& other) {
        //std::swap(fmtId, other.fmtId);
        //std::swap(entrySize, other.entrySize);
        //std::swap(timestamp, other.timestamp);
        //argData.swap(other.argData);
        //dyMem.swap(other.dyMem);

        fmtId = other.fmtId;
        entrySize = other.entrySize;
        timestamp = other.timestamp;
        memcpy(argData.data(), other.argData.data(), ArgBytes);
        dyMem.reset(other.dyMem.release());

        return *this;
    }
};

const int AddNum = 100000000;
const int ThreadNum = 1;
const size_t ArgNum = 32;

#ifndef USE_ACTIVE
const int RingBufSize = 16 * 1024;
const int batchSizePush = 128;
const int batchSizePop = 512;
#endif

std::thread Threads[ThreadNum];

int Result = 0;


typedef std::function< void() > Callback;

#ifndef USE_ACTIVE
/*Callback*/ UncompressedEntry<ArgNum> stageBufPop[batchSizePop];
/*Callback*/ const UncompressedEntry<ArgNum> stageBufPush[batchSizePush];
#endif

#ifdef USE_ACTIVE
auto activeQueue = Active::createActive();
#else
auto ringQueue = new spsc_queue</*Callback*/UncompressedEntry<ArgNum>, RingBufSize>();
#endif


void add() {
    int i = 0;

    while (i < AddNum) {
#ifdef USE_ACTIVE
        activeQueue->send([&] { ++Result; });
        i++;
#else
        UncompressedEntry<ArgNum> myMsg/*(128)*/;
        ringQueue->wait_push(/*[&] { ++Result; }*/std::move(myMsg));
        i++;
        //bool stat = ringQueue->try_push([&] { ++Result; });
        //if (stat) i++;
        /*int n = ringQueue->push(stageBufPush, batchSizePush);
        i += n;*/
#endif 
    }

}

#ifndef USE_ACTIVE
void RingBufBG() {
    //Callback func;
    int j = 0;
    while (/*Result != AddNum * ThreadNum*/ j < AddNum * ThreadNum) {
        /*bool stat = ringQueue->pop(func);
        if (stat) func();*/

        int n = ringQueue->pop(stageBufPop, batchSizePop);
        for (int i = 0; i < n; i++) {
            /*stageBufPop[i]()*/ char* p = stageBufPop[i].argData.data();
            p[5] = 'a';
        }

        j += n;
    }
}
#endif

int main() {

#ifndef USE_ACTIVE
    //for (int i = 0; i < batchSizePush; i++) {
    //    //stageBufPush[i] = [&] { ++Result; };
    //    UncompressedEntry<ArgNum> myMsg;
    //    stageBufPush[i] = std::move(myMsg);
    //}
#endif

    auto start = system_clock::now();

    for (int i = 0; i < ThreadNum; i++) {
        Threads[i] = std::thread(add);
    }

#ifndef USE_ACTIVE
    std::thread RingBufBGThrread(RingBufBG);
#endif

    for (int i = 0; i < ThreadNum; i++) {
        Threads[i].join();
    }

    std::cout << "test" << std::endl;

    auto end = system_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    cout << "cost: "
        << double(duration.count()) * microseconds::period::num / microseconds::period::den << " seconds" << endl;

#ifndef USE_ACTIVE
    RingBufBGThrread.join();
#endif


#ifdef USE_ACTIVE
    //while (Result != AddNum * ThreadNum) { std::cout << Result << std::endl; }
#endif

    std::cout << "Result: " << Result << std::endl;

    end = system_clock::now();
    duration = duration_cast<microseconds>(end - start);

    cout << "cost: "
        << double(duration.count()) * microseconds::period::num / microseconds::period::den << " seconds" << endl;


    return 0;
}