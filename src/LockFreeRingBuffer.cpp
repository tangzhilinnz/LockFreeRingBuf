#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <stdlib.h>
#include <memory>
#include <array>
#include <fstream>

#include "active.h"
#include "RingQueue.h"

//#include "mimalloc-new-delete.h"
//#include "mimalloc-override.h"
//#include "mimalloc.h"

using namespace std;
using namespace chrono;


//#define USE_ACTIVE

template <size_t ArgBytes>
struct UncompressedEntry {
    // Uniquely identifies a log message by its format string and file
    // location, assigned at compile time by the preprocessor.
    uint32_t fmtId{ 0 };

    // Number of bytes for this header and the various uncompressed
    // log arguments after it
    uint32_t entrySize{ 0 };

    // Stores the rdtsc() value at the time of the log function invocation
    uint64_t timestamp{ 0 };

    //char argData[0];

    // After this header are the uncompressed arguments required by
    // the original format string
    //char argData[ArgBytes];
    //char* dyMem{nullptr};

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
        , dyMem(other.dyMem.release())/*dyMem(other.dyMem)*/{

        memcpy(argData.data(), other.argData.data(), ArgBytes);
        //memcpy(argData, other.argData, ArgBytes);
    }

    UncompressedEntry(UncompressedEntry&& other)
        : fmtId(other.fmtId)
        , entrySize(other.entrySize)
        , timestamp(other.timestamp)
        , dyMem(other.dyMem.release()) /*dyMem(other.dyMem)*/ {

        memcpy(argData.data(), other.argData.data(), ArgBytes);
        //memcpy(argData, other.argData, ArgBytes);
    }

    UncompressedEntry& operator=(const UncompressedEntry& other) {
        fmtId = other.fmtId;
        entrySize = other.entrySize;
        timestamp = other.timestamp;
        memcpy(argData.data(), other.argData.data(), ArgBytes);
        //memcpy(argData, other.argData, ArgBytes);
        dyMem.reset(other.dyMem.release());
        //dyMem = other.dyMem;

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
        //memcpy(argData, other.argData, ArgBytes);
        dyMem.reset(other.dyMem.release());
        //dyMem = other.dyMem;

        return *this;
    }
};

const int AddNum = 100000000;
const int ThreadNum = 1;
const size_t ArgNum = 24;

#ifndef USE_ACTIVE
const int RingBufSize = 16 * 1024;
const int batchSizePush = 128;
const int batchSizePop = 512;
#endif

std::thread Threads[ThreadNum];

int Result = 0;


typedef std::function< void() > Callback;

#ifndef USE_ACTIVE
/*Callback*/ // UncompressedEntry<ArgNum> stageBufPop[batchSizePop]{0};
//UncompressedEntry<ArgNum> sinkBufPop[batchSizePop]{ 0 };
/*Callback*/ //const UncompressedEntry<ArgNum> stageBufPush[batchSizePush]{0};
//std::queue<UncompressedEntry<ArgNum>> cachePop{};
#endif

#ifdef USE_ACTIVE
auto activeQueue = Active::createActive();
#else
std::vector< spsc_queue<UncompressedEntry<ArgNum>, RingBufSize>* > pStagingBuffers;
//auto ringQueue = new spsc_queue</*Callback*/UncompressedEntry<ArgNum>, RingBufSize>();
//auto sinkQueue = new spsc_queue</*Callback*/UncompressedEntry<ArgNum>, RingBufSize * 128>();
std::unique_ptr<std::ofstream> outptr;
#endif


void add(int index) {
    int i = 0;

    while (i < AddNum/* + 1000*/) {
#ifdef USE_ACTIVE
        activeQueue->send([&] { ++Result; });
        i++;
#else
        UncompressedEntry<ArgNum> myMsg/*(256)*/;
        //char tmp[64];
        //UncompressedEntry* p = (UncompressedEntry *)tmp;
        //p->fmtId = 0;
        /*ringQueue*/pStagingBuffers.at(index)->wait_push(/*[&] { ++Result; }*/std::move(myMsg));
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
    long j = 0;
    UncompressedEntry<ArgNum> stageBufPop[batchSizePop];
    while (/*Result != AddNum * ThreadNum*/ j < AddNum * ThreadNum) {
        /*bool stat = ringQueue->pop(func);
        if (stat) func();*/
        /*UncompressedEntry<ArgNum>* pStageBufPop = stageBufPop;*/
        int nPerItr = 0;

        for (int index = 0; index < ThreadNum; index++) {
            int n = /*ringQueue*/pStagingBuffers.at(index)->pop(/*pStageBufPop*/stageBufPop, batchSizePop);
            //for (int i = 0; i < n; i++) {
            //    /*stageBufPop[i]()*/ char* p = stageBufPop[i].argData.data();
            //    p[5] = 'a';
            //}
            //pStageBufPop = &stageBufPop[n];

            //for (int i = 0; i < n; i++) {
            //    cachePop.push/*emplace_back*/(std::move(stageBufPop[i]));
            //}
            //cachePop.clear();

            //int tmp = n;
            //UncompressedEntry<ArgNum>* pStageBufPop = stageBufPop;

            //int count = 0;
            //while (tmp > 0) {
            //    int num = sinkQueue->push(pStageBufPop, tmp);
            //    tmp -= num;
            //    pStageBufPop += num;
            //    //std::cout << ++count << std::endl;
            //}

            nPerItr += n;
        }

        j += nPerItr;

        //std::cout << "";
    }
}

//void sinkBufBG() {
//    //Callback func;
//    UncompressedEntry<ArgNum> sinkBufPop[batchSizePop];
//    int j = 0;
//    while (/*Result != AddNum * ThreadNum*/ j < AddNum * ThreadNum) {
//
//        int n = sinkQueue->pop(/*pStageBufPop*/sinkBufPop, batchSizePop);
//
//        cachePop.pop();
//
//        //for (int i = 0; i < n; i++) {
//        //    /*stageBufPop[i]()*/ char* p = sinkBufPop[i].argData.data();
//        //    p[5] = 'a';
//
//        //    *outptr.get() << p[5];
//        //}
//
//        j += n;
//    }
//}
#endif

int main() {

#ifndef USE_ACTIVE
    //for (int i = 0; i < batchSizePush; i++) {
    //    //stageBufPush[i] = [&] { ++Result; };
    //    UncompressedEntry<ArgNum> myMsg;
    //    stageBufPush[i] = std::move(myMsg);
    //}
    outptr.reset(new std::ofstream);
    std::ios_base::openmode mode = std::ios_base::out;
    mode |= std::ios_base::trunc/*app*/;
    outptr.get()->open("./test.log", mode);

    //cachePop.reserve(RingBufSize * 128);
    for (int i = 0; i < ThreadNum; i++) {
        pStagingBuffers.push_back(new spsc_queue<UncompressedEntry<ArgNum>, RingBufSize>);
    }
#endif

    auto start = system_clock::now();

    for (int i = 0; i < ThreadNum; i++) {
        Threads[i] = std::thread(add, i);
    }

#ifndef USE_ACTIVE
    std::thread RingBufBGThrread(RingBufBG);
    //std::thread sinkBufBGThrread(sinkBufBG);
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
    //sinkBufBGThrread.join();
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