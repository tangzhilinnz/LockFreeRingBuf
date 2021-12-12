#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <stdlib.h>
#include <memory>
//#include <array>
#include <fstream>
#include <mutex>
#include <ctime>
#include <functional>
#include <condition_variable>
#include <time.h>
#include <sys/time.h>
#include <future>

//#include <sched.h>
//#include <pthread.h>



//#include <unistd.h>

//#include "active.h"
#include "RingQueue.h"
#include "SinkBuffer.h"
//#include "Cycles.h"

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
    //struct timeval tv;

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

    //UncompressedEntry() {}

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

const int AddNum = /*33333333*/100000000;
const int ThreadNum = 1;
const size_t ArgNum = 4;

#ifndef USE_ACTIVE
const int RingBufSize = 16 * 1024;
//const int batchSizePush = 128;
const int batchSizePop = 1024;
#endif

std::thread Threads[ThreadNum];
std::thread RingBufBGThrread;
std::thread sinkBufBGThrread;

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

//const int OutBufSize = 64 * 1024 * 1024;

//std::unique_ptr<char[]> pOutBufUptr{ new char[OutBufSize] };

const int SinkBufferSize = 64 * 1024 * 1024;
const int CacheBufQueueSize = 8;
//const int SinkBufQueueSize = 32;
//static thread_local spsc_queue<UncompressedEntry<ArgNum>, RingBufSize>* pThdLocalSBuf;
typedef SinkBuffer<SinkBufferSize> Buffer;
//typedef spsc_queue<Buffer*, CacheBufQueueSize> TcacheBuffferQueue;
//typedef spsc_queue<Buffer*, SinkBufQueueSize> TsinkBuffferQueue;



//std::unique_ptr<TcacheBuffferQueue> cacheBufQueueUptr = std::make_unique<TcacheBuffferQueue>();
//std::unique_ptr<TsinkBuffferQueue> sinkBufQueueUptr = std::make_unique<TsinkBuffferQueue>();
typedef std::queue<Buffer*> TcacheBuffferQueue;
typedef std::queue<Buffer*> TsinkBuffferQueue;

TcacheBuffferQueue cacheBufQueue;
TsinkBuffferQueue  sinkBufQueue;


std::condition_variable workAdded;
std::mutex condMutex;
std::condition_variable workAdded1;
std::mutex condMutex1;

std::mutex mutexSink;

std::mutex mutexBuf;

bool sinkBufBGExit = false;

std::unique_ptr<std::ofstream> outptr;
#endif

//uint64_t f() {
//    return Cycles::rdtsc();
//}

void add(int index) {

    //cpu_set_t cpuset;
    //CPU_ZERO(&cpuset);
    //CPU_SET(index, &cpuset);
    //int rc = pthread_setaffinity_np(Threads[index].native_handle(), sizeof(cpu_set_t), &cpuset);
    //if (rc != 0) {
    //    std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    //}
    //std::this_thread::sleep_for(std::chrono::milliseconds(1));


    int i = 0;

    auto pThdLocalSBuf = pStagingBuffers.at(index);

    //std::this_thread::sleep_for(std::chrono::microseconds(100));

    //{
    //    std::lock_guard<std::mutex> lock(mutexBuf);
    //    pStagingBuffers.push_back(pThdLocalSBuf); 
    //}

    while (i < AddNum/* + 1000*/) {

        //std::cout << i << std::endl;
#ifdef USE_ACTIVE
        activeQueue->send([&] { ++Result; });
        i++;
#else
        //auto high_resolution_time_point = std::move(std::chrono::high_resolution_clock::now());
        //time_t t = std::time(NULL);
        //auto start = std::clock();


        //struct timeval tv;
        //gettimeofday(&tv, NULL);
        auto t = Cycles::rdtsc();
        //auto timeStamp  = std::chrono::steady_clock::now();
        //int t = 0;
        //struct timespec time1;
        //clock_gettime(CLOCK_MONOTONIC, &time1);
        //clock_t timestamp = clock();
        //std::cout << timestamp << std::endl;

        //struct timeval  tv1;
        //gettimeofday(&tv1, NULL);

        auto pSpace = pThdLocalSBuf->reserve_1();
        //UncompressedEntry<ArgNum>* pSpace;
        //while (!(pSpace = pThdLocalSBuf->try_reserve_1())) {
        //    std::cout << "" << "";
        //}

        pSpace->entrySize = ArgNum + 12;

        pSpace->fmtId = 7;
        pSpace->timestamp = t;
        pThdLocalSBuf->finish_1();

        i++;
        //std::this_thread::sleep_for(std::chrono::microseconds(0));
        /*bool stat = ringQueue->try_push([&] { ++Result; });
        if (stat) i++;*/
        /*int n = ringQueue->push(stageBufPush, batchSizePush);
        i += n;*/
#endif 
    }
}


int allocNum = 0;

#ifndef USE_ACTIVE
void RingBufBG() {
    //Callback func;

    //std::this_thread::sleep_for(std::chrono::microseconds(100));
    long j = 0;

    //cpu_set_t cpuset;
    //CPU_ZERO(&cpuset);
    //CPU_SET(2, &cpuset);
    //int rc = pthread_setaffinity_np(RingBufBGThrread.native_handle(), sizeof(cpu_set_t), &cpuset);
    //if (rc != 0) {
    //    std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    //}
    //std::this_thread::sleep_for(std::chrono::milliseconds(10));

    UncompressedEntry<ArgNum>* pStageBufPop;

    Buffer* pBuffer = nullptr;

    {
        std::lock_guard<std::mutex> lock(mutexSink);
        pBuffer = cacheBufQueue.front();
        cacheBufQueue.pop();
    }

    while (j < AddNum * ThreadNum) {

        /*bool stat = ringQueue->pop(func);
        if (stat) func();*/
        /*UncompressedEntry<ArgNum>* pStageBufPop = stageBufPop;*/

        int nPerItr = 0;

        for (int index = 0; index < ThreadNum; index++) {
            int n = 0;
            pStageBufPop = nullptr;
            pStageBufPop = pStagingBuffers.at(index)->peek(&n, batchSizePop);

            if (n == 0) continue;

            int count = n;

            //std::cout << count << std::endl;

            while (count > 0) {
                if ((unsigned int)pBuffer->avail() > pStageBufPop->entrySize) {
                    char* p = pBuffer->current();

                    *(uint32_t*)p = pStageBufPop->fmtId;
                    *(uint64_t*)(p + 4) = pStageBufPop->timestamp;
                    //*(struct timeval*)(p + 4) = pStageBufPop->tv;

                    pBuffer->add(12);

                    //memcpy(p + 12, pStageBufPop->argData, ArgNum);

                    //p += 12 + ArgNum;

                    pBuffer->append(pStageBufPop->argData, ArgNum);

                    ++pStageBufPop;
                    if (pStageBufPop == pStagingBuffers.at(index)->end_buf())
                        pStageBufPop = (UncompressedEntry<ArgNum>*)pStagingBuffers.at(index)->begin_buf();

                    --count;
                }
                else {
                    //auto start = system_clock::now();                
                    {
                        std::lock_guard<std::mutex> lock(mutexSink);

                        sinkBufQueue.push(pBuffer);

                        if (cacheBufQueue.size() != 0) {
                            pBuffer = cacheBufQueue.front();
                            cacheBufQueue.pop();
                        }

                        else {
                            pBuffer = new Buffer;
                            allocNum++;
                        }
                    }


                    //auto end = system_clock::now();
                    //auto duration = duration_cast<microseconds>(end - start);
                    //cout << "cost_count:"
                    //     << double(duration.count()) << endl;
                }
            }


            pStagingBuffers.at(index)->consume(n);

            nPerItr += n;
        }
                

        j += nPerItr;

        if (nPerItr == 0) {
            std::unique_lock<std::mutex> lock(condMutex);
            workAdded.wait_for(lock, std::chrono::microseconds(1));
        }
        //else {
        //    //auto cTime = Cycles::rdtsc();
        //    //auto pTime = pBuffer->getLastCycles();
        //    if (Cycles::rdtsc() - pBuffer->getLastCycles() > Cycles::getCyclesPerSec()) {

        //        //std::cout << Cycles::getCyclesPerSec() << std::endl;
        //        pBuffer->resetCycles();

        //        std::lock_guard<std::mutex> lock(mutexSink);

        //        sinkBufQueue.push(pBuffer);

        //        if (cacheBufQueue.size() != 0) {
        //            pBuffer = cacheBufQueue.front();
        //            cacheBufQueue.pop();
        //        }

        //        else {
        //            pBuffer = new Buffer;
        //            allocNum++;
        //        }
        //    }
        //}
        //std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));
    sinkBufBGExit = true;
}

void sinkBufBG() {

    while (!sinkBufBGExit) {

        Buffer* pBuffer = nullptr;

        if (sinkBufQueue.size() > 0) {

            {
                std::lock_guard<std::mutex> lock(mutexSink);
                pBuffer = sinkBufQueue.front();
                sinkBufQueue.pop();
            }


            outptr.get()->write(pBuffer->data(), pBuffer->length());
            //outptr.get()->flush();
            pBuffer->reset(); //reset SinkBuffer container


            if (cacheBufQueue.size() >= CacheBufQueueSize)
                delete pBuffer;
            else {
                std::lock_guard<std::mutex> lock(mutexSink);
                cacheBufQueue.push(pBuffer);
            }
        }
        else {
            std::unique_lock<std::mutex> lock(condMutex1);
            workAdded1.wait_for(lock, std::chrono::microseconds(1));
        }
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

    for (int i = 0; i < CacheBufQueueSize; i++) {
        cacheBufQueue.push(new Buffer);
    }


    outptr.reset(new std::ofstream);
    std::ios_base::openmode mode = std::ios_base::out;
    mode |= std::ios_base::trunc/*app*/;
    outptr.get()->open("./fk.log", mode);

    //std::cout << "test" << std::endl;

    for (int i = 0; i < ThreadNum; i++) {
        pStagingBuffers.push_back(new spsc_queue<UncompressedEntry<ArgNum>, RingBufSize>);
    }
#endif


#ifndef USE_ACTIVE
    RingBufBGThrread = std::thread(RingBufBG);
    sinkBufBGThrread = std::thread(sinkBufBG);
#endif
    //std::this_thread::sleep_for(std::chrono::seconds(10));
    auto start = system_clock::now();

    for (int i = 0; i < ThreadNum; i++) {
        Threads[i] = std::thread(add, i);
    }

    //auto handle = std::async(std::launch::async, add, 0);

    std::cout << "test" << std::endl;

    for (int i = 0; i < ThreadNum; i++) {
        Threads[i].join();
    }

    //handle.wait();

    auto end = system_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    cout << "cost: "
        << double(duration.count()) * microseconds::period::num / microseconds::period::den << " seconds" << endl;

#ifndef USE_ACTIVE
    RingBufBGThrread.join();
    sinkBufBGThrread.join();
#endif


#ifdef USE_ACTIVE
    //while (Result != AddNum * ThreadNum) { std::cout << Result << std::endl; }
#endif

    cout << "Result: " << Result << std::endl;

    end = system_clock::now();
    duration = duration_cast<microseconds>(end - start);

    cout << "cost: "
        << double(duration.count()) * microseconds::period::num / microseconds::period::den << " seconds" << endl;
    cout << "allocNum: " << allocNum << std::endl;

    cout << "cacheBufQueue.size(): " << cacheBufQueue.size() << std::endl;
    cout << "sinkBufQueue.size(): " << sinkBufQueue.size() << std::endl;


    return 0;
}