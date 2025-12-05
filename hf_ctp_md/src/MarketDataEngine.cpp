#include "MarketDataEngine.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <pthread.h>
#include <immintrin.h> // _mm_pause
#include <vector>
#include <numeric>
#include <algorithm>

// RDTSC 辅助函数
static inline uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

MarketDataEngine::MarketDataEngine(SPSCQueue<MdData>* pQueue)
    : m_pQueue(pQueue), m_running(false), m_cpu_id(-1) {
}

MarketDataEngine::~MarketDataEngine() {
    stop();
}

void MarketDataEngine::start() {
    if (m_running) return;
    m_running = true;
    m_thread = std::thread(&MarketDataEngine::run, this);
}

void MarketDataEngine::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void MarketDataEngine::set_cpu_affinity(int cpu_id) {
    m_cpu_id = cpu_id;
}

void MarketDataEngine::run() {

    std::cout << "[StrategyThread] Engine started. Polling queue..." << std::endl;

    MdData md;
    long long count = 0;
    
    // 统计缓存
    const int STAT_BATCH = 100; // 每100个包统计一次
    std::vector<uint64_t> latency_samples;
    latency_samples.reserve(STAT_BATCH);

    while (m_running) {
        if (m_pQueue->pop(md)) {
            // === 关键路径：无IO ===
            
            uint64_t process_tsc = rdtsc();
            uint64_t latency_cycles = process_tsc - md.receive_tsc;
            
            // 仅收集数据，不打印
            latency_samples.push_back(latency_cycles);
            count++;

            // 批量统计打印 (不在关键路径上频繁做)
            if (latency_samples.size() >= STAT_BATCH) {
                uint64_t sum = std::accumulate(latency_samples.begin(), latency_samples.end(), 0ULL);
                uint64_t min = *std::min_element(latency_samples.begin(), latency_samples.end());
                uint64_t max = *std::max_element(latency_samples.begin(), latency_samples.end());
                double avg = (double)sum / STAT_BATCH;

                std::cout << "[Strategy] Processed " << count << " ticks. "
                          << "Latency(Cycles) Min:" << min 
                          << " Max:" << max 
                          << " Avg:" << avg << std::endl;
                
                latency_samples.clear();
            }
        } else {
            _mm_pause(); 
        }
    }
    
    std::cout << "[StrategyThread] Engine stopped. Processed " << count << " ticks." << std::endl;
}
