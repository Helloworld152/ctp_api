#include <iostream>
#include <string>
#include <thread>
#include <csignal>
#include "ThostFtdcMdApi.h"
#include "SPSCQueue.h"
#include "CTPMdSpi.h"
#include "MarketDataEngine.h"

// 全局标志位，用于信号处理
std::atomic<bool> g_running(true);

void signal_handler(int signal) {
    std::cout << "\n[Main] Caught signal " << signal << ", stopping..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    // 注册信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== High Frequency CTP Market Data System ===" << std::endl;

    // 1. 初始化无锁队列
    // 容量设为 1024 (必须是2的幂次如果做位运算优化，但我们的实现里用取模，稍微宽容点)
    // 考虑到行情突发流量，设大一点比较安全，例如 4096
    std::cout << "[Main] Initializing Ring Buffer (size=4096)..." << std::endl;
    SPSCQueue<MdData> queue(4096);

    // 2. 初始化并启动消费者引擎
    std::cout << "[Main] Starting Strategy Engine..." << std::endl;
    MarketDataEngine engine(&queue);
    // 去除绑核设置
    // engine.set_cpu_affinity(1); 
    engine.start();

    // 3. 初始化 CTP API
    std::cout << "[Main] Initializing CTP API..." << std::endl;
    CThostFtdcMdApi* pMdApi = CThostFtdcMdApi::CreateFtdcMdApi("./flow/", false, false);
    if (!pMdApi) {
        std::cerr << "[Main] Failed to create MdApi instance." << std::endl;
        return -1;
    }

    CTPMdSpi spi(pMdApi, &queue);
    pMdApi->RegisterSpi(&spi);
    
    // 模拟环境地址 (从原代码获取)
    char frontAddr[] = "tcp://101.231.162.58:41213"; 
    pMdApi->RegisterFront(frontAddr);
    
    pMdApi->Init();
    
    // 等待连接完成 (实际应由回调驱动，这里简单 sleep)
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 4. 登录
    // 模拟环境账号
    spi.ReqUserLogin("9999", "247060", "RY20000219*");

    // 等待登录完成
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 5. 订阅行情
    // 为了更快地触发 100 次统计，我们可以多订几个活跃合约
    char* instruments[] = {
        (char*)"au2512", (char*)"ag2512", (char*)"rb2601", (char*)"TS2601",
        (char*)"cu2601", (char*)"al2601", (char*)"zn2601", (char*)"ni2601"
    };
    spi.SubscribeMarketData(instruments, 8);

    std::cout << "[Main] System running. Press Ctrl+C to exit." << std::endl;

    // 6. 主线程保持运行，直到收到信号
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 7. 清理资源
    std::cout << "[Main] Shutting down..." << std::endl;
    
    // 先停 API，不再产生新数据
    pMdApi->RegisterSpi(nullptr);
    pMdApi->Release();
    pMdApi = nullptr; // Release 会自删除

    // 再停消费者
    engine.stop();

    std::cout << "[Main] Shutdown complete." << std::endl;
    return 0;
}