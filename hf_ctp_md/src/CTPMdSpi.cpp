#include "CTPMdSpi.h"
#include <chrono>
#include <pthread.h>

CTPMdSpi::CTPMdSpi(CThostFtdcMdApi* pUserApi, SPSCQueue<MdData>* pQueue)
    : m_pUserApi(pUserApi), m_pQueue(pQueue), m_requestId(0) {
}

CTPMdSpi::~CTPMdSpi() {
}

void CTPMdSpi::OnFrontConnected() {
    // 不再绑核，仅打印连接信息
    std::cout << "[CTPThread] Front Connected." << std::endl;
}

void CTPMdSpi::OnFrontDisconnected(int nReason) {
    std::cout << "[MainThread] Front Disconnected. Reason: " << nReason << std::endl;
}

void CTPMdSpi::ReqUserLogin(const char* brokerId, const char* userId, const char* password) {
    CThostFtdcReqUserLoginField req;
    memset(&req, 0, sizeof(req));
    strcpy(req.BrokerID, brokerId);
    strcpy(req.UserID, userId);
    strcpy(req.Password, password);
    
    int ret = m_pUserApi->ReqUserLogin(&req, ++m_requestId);
    if (ret != 0) {
        std::cerr << "[MainThread] ReqUserLogin failed: " << ret << std::endl;
    }
}

void CTPMdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        std::cerr << "[MainThread] Login Failed: " << pRspInfo->ErrorMsg << std::endl;
    } else {
        std::cout << "[MainThread] Login Success. TradingDay: " << pRspUserLogin->TradingDay << std::endl;
    }
}

void CTPMdSpi::SubscribeMarketData(char* ppInstrumentID[], int nCount) {
    int ret = m_pUserApi->SubscribeMarketData(ppInstrumentID, nCount);
    if (ret != 0) {
        std::cerr << "[MainThread] SubscribeMarketData failed: " << ret << std::endl;
    }
}

void CTPMdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        std::cerr << "[MainThread] Subscribe Failed: " << pRspInfo->ErrorMsg << std::endl;
    } else {
        std::cout << "[MainThread] Subscribe Success: " << (pSpecificInstrument ? pSpecificInstrument->InstrumentID : "null") << std::endl;
    }
}

// 辅助函数：读取 CPU 时间戳计数器
static inline uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

// === 关键路径 ===
void CTPMdSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData) {
    if (!pDepthMarketData) return;

    MdData md;
    // 1. 极速记录时间 (RDTSC 指令)
    md.receive_tsc = rdtsc();

    // 2. 数据拷贝
    std::memcpy(&md.data, pDepthMarketData, sizeof(CThostFtdcDepthMarketDataField));

    // 3. 推入无锁队列
    m_pQueue->push(md);
}