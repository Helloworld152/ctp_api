#pragma once

#include "SPSCQueue.h"
#include "CTPMdSpi.h"
#include <thread>
#include <atomic>

class MarketDataEngine {
public:
    MarketDataEngine(SPSCQueue<MdData>* pQueue);
    ~MarketDataEngine();

    void start();
    void stop();
    
    // 设置线程亲和性 (绑定 CPU 核心)
    void set_cpu_affinity(int cpu_id);

private:
    void run();

private:
    SPSCQueue<MdData>* m_pQueue;
    std::thread m_thread;
    std::atomic<bool> m_running;
    int m_cpu_id;
};
