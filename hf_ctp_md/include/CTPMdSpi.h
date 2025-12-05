#pragma once

#include "ThostFtdcMdApi.h"
#include "SPSCQueue.h"
#include <cstring>
#include <iostream>

// 定义用于队列传输的数据结构
struct MdData {
    CThostFtdcDepthMarketDataField data;
    uint64_t receive_tsc; // 接收时的 CPU 周期数 (RDTSC)
};

class CTPMdSpi : public CThostFtdcMdSpi {
public:
    CTPMdSpi(CThostFtdcMdApi* pUserApi, SPSCQueue<MdData>* pQueue);
    virtual ~CTPMdSpi();

    // CTP 回调接口
    virtual void OnFrontConnected() override;
    virtual void OnFrontDisconnected(int nReason) override;
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;
    virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;
    
    // 核心回调：深度行情通知
    virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData) override;

public:
    // 业务接口
    void ReqUserLogin(const char* brokerId, const char* userId, const char* password);
    void SubscribeMarketData(char* ppInstrumentID[], int nCount);

private:
    CThostFtdcMdApi* m_pUserApi;
    SPSCQueue<MdData>* m_pQueue; // 无锁队列指针
    int m_requestId;
};
