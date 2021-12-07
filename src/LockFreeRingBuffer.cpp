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
#include "SinkBuffer.h"

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
    char argData[ArgBytes];
    char* dyMem{ nullptr };

    //std::array<char, ArgBytes> argData{0};

    //mutable  std::unique_ptr<char[]> dyMem{nullptr};

    /*UncompressedEntry(int numBytesDym)
        : dyMem(new char[numBytesDym]) {
    }*/

    UncompressedEntry() {}

    //~UncompressedEntry() {}

    // UncompressedEntry(const UncompressedEntry& other) = delete;
    // UncompressedEntry& operator=(const UncompressedEntry& other) = delete;

    //UncompressedEntry(const UncompressedEntry& other)
    //    : fmtId(other.fmtId)
    //    , entrySize(other.entrySize)
    //    , timestamp(other.timestamp)
    //    , dyMem(other.dyMem.release())/*dyMem(other.dyMem)*/ {

    //    memcpy(argData.data(), other.argData.data(), ArgBytes);
    //    //memcpy(argData, other.argData, ArgBytes);
    //}

    //UncompressedEntry(UncompressedEntry&& other)
    //    : fmtId(other.fmtId)
    //    , entrySize(other.entrySize)
    //    , timestamp(other.timestamp)
    //    , dyMem(other.dyMem.release()) /*dyMem(other.dyMem)*/ {

    //    memcpy(argData.data(), other.argData.data(), ArgBytes);
    //    //memcpy(argData, other.argData, ArgBytes);
    //}

    //UncompressedEntry& operator=(const UncompressedEntry& other) {
    //    fmtId = other.fmtId;
    //    entrySize = other.entrySize;
    //    timestamp = other.timestamp;
    //    memcpy(argData.data(), other.argData.data(), ArgBytes);
    //    //memcpy(argData, other.argData, ArgBytes);
    //    dyMem.reset(other.dyMem.release());
    //    //dyMem = other.dyMem;

    //    return *this;
    //}

    //UncompressedEntry& operator=(UncompressedEntry&& other) {
    //    //std::swap(fmtId, other.fmtId);
    //    //std::swap(entrySize, other.entrySize);
    //    //std::swap(timestamp, other.timestamp);
    //    //argData.swap(other.argData);
    //    //dyMem.swap(other.dyMem);

    //    fmtId = other.fmtId;
    //    entrySize = other.entrySize;
    //    timestamp = other.timestamp;
    //    memcpy(argData.data(), other.argData.data(), ArgBytes);
    //    //memcpy(argData, other.argData, ArgBytes);
    //    dyMem.reset(other.dyMem.release());
    //    //dyMem = other.dyMem;

    //    return *this;
    //}
};

const int AddNum = 20000000;
const int ThreadNum = 5;
const size_t ArgNum = 16;

#ifndef USE_ACTIVE
const int RingBufSize = 16 * 1024;
//const int batchSizePush = 128;
const int batchSizePop = 1024;
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

const int OutBufSize = 32 * 1024 * 1024;

std::unique_ptr<char[]> pOutBufUptr{new char[OutBufSize]};

std::unique_ptr<std::ofstream> outptr;
#endif


void add(int index) {
    int i = 0;

    auto pThdLocalSBuf = pStagingBuffers.at(index);

    while (i < AddNum/* + 1000*/) {

        //std::cout << i << std::endl;
#ifdef USE_ACTIVE
        activeQueue->send([&] { ++Result; });
        i++;
#else
        //UncompressedEntry<ArgNum> myMsg/*(256)*/;
        //pStagingBuffers.at(index)->wait_push(std::move(myMsg));
        //i++;

        auto pSpace = pThdLocalSBuf->reserve_1();
        pSpace->entrySize = ArgNum + 12;
        pSpace->timestamp = 0;
        pSpace->fmtId = 7;
        //pSpace->dyMem.reset(new char[32]);
        pThdLocalSBuf->finish_1();

        i++;

        /*bool stat = ringQueue->try_push([&] { ++Result; });
        if (stat) i++;*/
        /*int n = ringQueue->push(stageBufPush, batchSizePush);
        i += n;*/
#endif 
    }

}

#ifndef USE_ACTIVE
void RingBufBG() {
    //Callback func;
    long j = 0;

    char* p = pOutBufUptr.get();
    unsigned int remain = OutBufSize;

    int allocNum = 0;

    UncompressedEntry<ArgNum>* pStageBufPop;
    //UncompressedEntry<ArgNum> stageBufPop[batchSizePop];
    while (/*Result != AddNum * ThreadNum*/ j < AddNum * ThreadNum) {
        /*bool stat = ringQueue->pop(func);
        if (stat) func();*/
        /*UncompressedEntry<ArgNum>* pStageBufPop = stageBufPop;*/
        int nPerItr = 0;

        for (int index = 0; index < ThreadNum; index++) {
            int n = 0;
            pStageBufPop = nullptr;
            pStageBufPop = pStagingBuffers.at(index)->peek(&n, batchSizePop);

            if (n == 0) continue;

            //std::cout << "" << "" << "" << "";

            int count = n;

            //std::cout << count << std::endl;

            while (count > 0) {

                if (remain >= pStageBufPop->entrySize) {
                    *(uint32_t*)p = pStageBufPop->fmtId;
                    *(uint64_t*)(p + 4) = pStageBufPop->timestamp;

                    memcpy(p + 12, pStageBufPop->argData, ArgNum);

                    p += 12 + ArgNum;

                    ++pStageBufPop;
                    if (pStageBufPop == pStagingBuffers.at(index)->end_buf())
                        pStageBufPop = (UncompressedEntry<ArgNum>*)pStagingBuffers.at(index)->begin_buf();

                    --count;
                    remain -= 12 + ArgNum;
                }
                else {
                    //std::cout << pStageBufPop->entrySize << std::endl;
                    p = pOutBufUptr.get();
                    remain = OutBufSize;
                    ++allocNum;
                }
            }

            pStagingBuffers.at(index)->consume(n);

            nPerItr += n;
        }

        //std::cout << "" << "" << "" << "";

        j += nPerItr;

        //std::cout << "";
    }
    std::cout << "allocNum: " << allocNum << std::endl;
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

    //std::cout << "test" << std::endl;

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

    std::cout << "test" << std::endl;

    for (int i = 0; i < ThreadNum; i++) {
        Threads[i].join();
    }

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