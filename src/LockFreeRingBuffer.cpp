#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <mutex>

#include "active.h"
#include "RingQueue.h"

// #include "mimalloc-new-delete.h"
// #include "mimalloc-override.h"
// #include "mimalloc.h"

using namespace std;
using namespace chrono;


//#define USE_ACTIVE


const int AddNum = 100000000;
const int ThreadNum = 1;

#ifndef USE_ACTIVE
const int RingBufSize = 16 * 1024;
const int batchSizePush = 512;
const int batchSizePop = 512;
#endif

std::thread Threads[ThreadNum];

int Result = 0;


typedef std::function< void() > Callback;

#ifndef USE_ACTIVE
Callback stageBufPop[batchSizePop];
Callback stageBufPush[batchSizePush];
#endif

#ifdef USE_ACTIVE
auto activeQueue = Active::createActive();
#else
auto ringQueue = new spsc_queue<Callback, RingBufSize>();
#endif


void add() {
    int i = 0;

    while (i < AddNum) {
#ifdef USE_ACTIVE
        activeQueue->send([&] { ++Result; });
        i++;
#else
        bool stat = ringQueue->push([&] { ++Result; });
        if (stat) i++;
        // int n = ringQueue->push(stageBufPush, batchSizePush);
        // i += n;
#endif 
    }

}

#ifndef USE_ACTIVE
void RingBufBG() {
    Callback func;
    while (Result != AddNum * ThreadNum) {
        /*bool stat = ringQueue->pop(func);
        if (stat) func();*/

        int n = ringQueue->pop(stageBufPop, batchSizePop);
        for (int i = 0; i < n; i++) {
            stageBufPop[i]();
        }
    }
}
#endif

int main() {

#ifndef USE_ACTIVE
    for (int i = 0; i < batchSizePush; i++) {
        stageBufPush[i] = [&] { ++Result; };
    }
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

#ifndef USE_ACTIVE
    RingBufBGThrread.join();
#endif


#ifdef USE_ACTIVE
    while (Result != AddNum * ThreadNum) { std::cout << Result << std::endl; }
#endif

    std::cout << "Result: " << Result << std::endl;

    auto end = system_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    cout << "cost: "
         << double(duration.count()) * microseconds::period::num / microseconds::period::den << " seconds" << endl;


	return 0;
}
