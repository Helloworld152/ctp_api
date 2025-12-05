#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <cstring>
#include <vector>
#include <iomanip>
#include <sstream>
#include <ctime>
#include "ThostFtdcMdApi.h"
#include "ThostFtdcTraderApi.h"

// 全局变量
CThostFtdcMdApi* g_pMdApi = nullptr;
CThostFtdcTraderApi* g_pTraderApi = nullptr;
bool g_bConnected = false;
bool g_bLoggedIn = false;
bool g_bTraderConnected = false;
bool g_bTraderLoggedIn = false;
int g_nRequestID = 0;
std::vector<std::string> g_instrumentList;

struct MarketData {
    // 基本信息
    std::string TradingDay;          // 交易日
    std::string InstrumentID;        // 合约代码
    std::string ExchangeID;          // 交易所代码
    std::string ExchangeInstID;      // 合约在交易所的代码
    
    // 价格信息
    double LastPrice;                // 最新价
    double PreSettlementPrice;       // 上次结算价
    double PreClosePrice;            // 昨收盘
    double OpenPrice;                // 今开盘
    double HighestPrice;             // 最高价
    double LowestPrice;              // 最低价
    double ClosePrice;               // 今收盘
    double SettlementPrice;          // 本次结算价
    double UpperLimitPrice;          // 涨停板价
    double LowerLimitPrice;          // 跌停板价
    double AveragePrice;             // 当日均价
    
    // 持仓和成交信息
    double PreOpenInterest;          // 昨持仓量
    double OpenInterest;              // 持仓量
    double Volume;                   // 数量
    double Turnover;                 // 成交金额
    
    // 虚实度
    double PreDelta;                 // 昨虚实度
    double CurrDelta;                // 今虚实度
    
    // 时间信息
    std::string UpdateTime;          // 最后修改时间
    int UpdateMillisec;              // 最后修改毫秒
    std::string ActionDay;           // 业务日期
    
    // 买盘（申买）- 使用vector
    std::vector<double> BidPrices;   // 申买价（1-5档）
    std::vector<int> BidVolumes;     // 申买量（1-5档）
    
    // 卖盘（申卖）- 使用vector
    std::vector<double> AskPrices;   // 申卖价（1-5档）
    std::vector<int> AskVolumes;     // 申卖量（1-5档）
};

// 行情回调类
class CMdSpi : public CThostFtdcMdSpi
{
private:
    CThostFtdcMdApi* m_pMdApi;

public:
    CMdSpi(CThostFtdcMdApi* pMdApi) : m_pMdApi(pMdApi) {}

    // 连接成功回调
    virtual void OnFrontConnected() override
    {
        std::cout << "=== 连接成功 ===" << std::endl;
        g_bConnected = true;
        
        // 登录请求
        CThostFtdcReqUserLoginField req;
        memset(&req, 0, sizeof(req));
        strcpy(req.BrokerID, "9999");           // 模拟环境经纪商代码
        strcpy(req.UserID, "247060");           // 用户代码
        strcpy(req.Password, "RY20000219*");         // 密码
        
        int ret = m_pMdApi->ReqUserLogin(&req, ++g_nRequestID);
        if (ret != 0) {
            std::cout << "登录请求失败，错误代码: " << ret << std::endl;
        }
    }

    // 连接断开回调
    virtual void OnFrontDisconnected(int nReason) override
    {
        std::cout << "=== 连接断开 ===" << std::endl;
        std::cout << "断开原因: " << nReason << std::endl;
        g_bConnected = false;
        g_bLoggedIn = false;
    }

    // 心跳超时警告
    virtual void OnHeartBeatWarning(int nTimeLapse) override
    {
        std::cout << "=== 心跳超时警告 ===" << std::endl;
        std::cout << "超时时间: " << nTimeLapse << "秒" << std::endl;
    }

    // 登录响应
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, 
                               CThostFtdcRspInfoField *pRspInfo, 
                               int nRequestID, bool bIsLast) override
    {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            std::cout << "=== 登录失败 ===" << std::endl;
            std::cout << "错误代码: " << pRspInfo->ErrorID << std::endl;
            std::cout << "错误信息: " << pRspInfo->ErrorMsg << std::endl;
        } else {
            std::cout << "\n=== 登录响应数据结构 ===" << std::endl;
            std::cout << "交易日: " << pRspUserLogin->TradingDay << std::endl;
            std::cout << "登录成功时间: " << pRspUserLogin->LoginTime << std::endl;
            std::cout << "经纪公司代码: " << pRspUserLogin->BrokerID << std::endl;
            std::cout << "用户代码: " << pRspUserLogin->UserID << std::endl;
            std::cout << "交易系统名称: " << pRspUserLogin->SystemName << std::endl;
            std::cout << "前置编号: " << pRspUserLogin->FrontID << std::endl;
            std::cout << "会话编号: " << pRspUserLogin->SessionID << std::endl;
            std::cout << "最大报单引用: " << pRspUserLogin->MaxOrderRef << std::endl;
            std::cout << "上期所时间: " << pRspUserLogin->SHFETime << std::endl;
            std::cout << "大商所时间: " << pRspUserLogin->DCETime << std::endl;
            std::cout << "郑商所时间: " << pRspUserLogin->CZCETime << std::endl;
            std::cout << "中金所时间: " << pRspUserLogin->FFEXTime << std::endl;
            std::cout << "能源中心时间: " << pRspUserLogin->INETime << std::endl;
            std::cout << "后台版本信息: " << pRspUserLogin->SysVersion << std::endl;
            std::cout << "广期所时间: " << pRspUserLogin->GFEXTime << std::endl;
            std::cout << "当前登录中心号: " << pRspUserLogin->LoginDRIdentityID << std::endl;
            std::cout << "用户所属中心号: " << pRspUserLogin->UserDRIdentityID << std::endl;
            std::cout << "----------------------------------------" << std::endl;
            g_bLoggedIn = true;
            
            // 订阅行情
            subscribeMarketData();
        }
    }

    // 订阅行情
    void subscribeMarketData()
    {
        char* ppInstrumentID[] = {
            // (char*)"ag2509",    // 白银主力合约
            // (char*)"al2509",    // 铝主力合约
            // (char*)"ad2601",     // 豆粕主力合约
            (char*)"TS2512",     // 石油沥青主力合约
            (char*)"au2601",    // 螺纹钢主力合约
        };
        
        int ret = m_pMdApi->SubscribeMarketData(ppInstrumentID, 1);
        if (ret == 0) {
            std::cout << "=== 订阅行情成功 ===" << std::endl;
        } else {
            std::cout << "=== 订阅行情失败，错误代码: " << ret << std::endl;
        }
    }

    // 行情数据回调
    virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData) override
    {
        // 获取当前毫秒时间戳并转换为日期时间格式
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        auto millisec = ms % 1000;
        
        std::time_t time_t = sec;
        std::tm* tm = std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << millisec;
        
        std::cout << "\n[收到行情时间: " << oss.str() << "]" << std::endl;
        
        if (pDepthMarketData) {
            std::cout << "\n=== 行情数据结构 ===" << std::endl;
            std::cout << "交易日: " << pDepthMarketData->TradingDay << std::endl;
            std::cout << "合约代码: " << pDepthMarketData->InstrumentID << std::endl;
            std::cout << "交易所代码: " << pDepthMarketData->ExchangeID << std::endl;
            std::cout << "最新价: " << pDepthMarketData->LastPrice << std::endl;
            std::cout << "上次结算价: " << pDepthMarketData->PreSettlementPrice << std::endl;
            std::cout << "昨收盘: " << pDepthMarketData->PreClosePrice << std::endl;
            std::cout << "昨持仓量: " << pDepthMarketData->PreOpenInterest << std::endl;
            std::cout << "今开盘: " << pDepthMarketData->OpenPrice << std::endl;
            std::cout << "最高价: " << pDepthMarketData->HighestPrice << std::endl;
            std::cout << "最低价: " << pDepthMarketData->LowestPrice << std::endl;
            std::cout << "数量: " << pDepthMarketData->Volume << std::endl;
            std::cout << "成交金额: " << pDepthMarketData->Turnover << std::endl;
            std::cout << "持仓量: " << pDepthMarketData->OpenInterest << std::endl;
            std::cout << "今收盘: " << pDepthMarketData->ClosePrice << std::endl;
            std::cout << "本次结算价: " << pDepthMarketData->SettlementPrice << std::endl;
            std::cout << "涨停板价: " << pDepthMarketData->UpperLimitPrice << std::endl;
            std::cout << "跌停板价: " << pDepthMarketData->LowerLimitPrice << std::endl;
            std::cout << "昨虚实度: " << pDepthMarketData->PreDelta << std::endl;
            std::cout << "今虚实度: " << pDepthMarketData->CurrDelta << std::endl;
            std::cout << "最后修改时间: " << pDepthMarketData->UpdateTime << std::endl;
            std::cout << "最后修改毫秒: " << pDepthMarketData->UpdateMillisec << std::endl;
            std::cout << "申买价一: " << pDepthMarketData->BidPrice1 << std::endl;
            std::cout << "申买量一: " << pDepthMarketData->BidVolume1 << std::endl;
            std::cout << "申买价二: " << pDepthMarketData->BidPrice2 << std::endl;
            std::cout << "申买量二: " << pDepthMarketData->BidVolume2 << std::endl;
            std::cout << "申买价三: " << pDepthMarketData->BidPrice3 << std::endl;
            std::cout << "申买量三: " << pDepthMarketData->BidVolume3 << std::endl;
            std::cout << "申买价四: " << pDepthMarketData->BidPrice4 << std::endl;
            std::cout << "申买量四: " << pDepthMarketData->BidVolume4 << std::endl;
            std::cout << "申买价五: " << pDepthMarketData->BidPrice5 << std::endl;
            std::cout << "申买量五: " << pDepthMarketData->BidVolume5 << std::endl;
            std::cout << "申卖价一: " << pDepthMarketData->AskPrice1 << std::endl;
            std::cout << "申卖量一: " << pDepthMarketData->AskVolume1 << std::endl;
            std::cout << "申卖价二: " << pDepthMarketData->AskPrice2 << std::endl;
            std::cout << "申卖量二: " << pDepthMarketData->AskVolume2 << std::endl;
            std::cout << "申卖价三: " << pDepthMarketData->AskPrice3 << std::endl;
            std::cout << "申卖量三: " << pDepthMarketData->AskVolume3 << std::endl;
            std::cout << "申卖价四: " << pDepthMarketData->AskPrice4 << std::endl;
            std::cout << "申卖量四: " << pDepthMarketData->AskVolume4 << std::endl;
            std::cout << "申卖价五: " << pDepthMarketData->AskPrice5 << std::endl;
            std::cout << "申卖量五: " << pDepthMarketData->AskVolume5 << std::endl;
            std::cout << "当日均价: " << pDepthMarketData->AveragePrice << std::endl;
            std::cout << "业务日期: " << pDepthMarketData->ActionDay << std::endl;
            std::cout << "----------------------------------------" << std::endl;
        }
    }

    // 订阅行情响应
    virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, 
                                   CThostFtdcRspInfoField *pRspInfo, 
                                   int nRequestID, bool bIsLast) override
    {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            std::cout << "=== 订阅行情失败 ===" << std::endl;
            std::cout << "合约: " << pSpecificInstrument->InstrumentID << std::endl;
            std::cout << "错误代码: " << pRspInfo->ErrorID << std::endl;
            std::cout << "错误信息: " << pRspInfo->ErrorMsg << std::endl;
        } else {
            std::cout << "=== 订阅行情成功 ===" << std::endl;
            std::cout << "合约: " << pSpecificInstrument->InstrumentID << std::endl;
        }
    }


};

// 交易回调类
class CTraderSpi : public CThostFtdcTraderSpi
{
private:
    CThostFtdcTraderApi* m_pTraderApi;

public:
    CTraderSpi(CThostFtdcTraderApi* pTraderApi) : m_pTraderApi(pTraderApi) {}

    // 连接成功回调
    virtual void OnFrontConnected() override
    {
        std::cout << "=== 交易API连接成功 ===" << std::endl;
        g_bTraderConnected = true;
        
        // 登录请求
        CThostFtdcReqUserLoginField req;
        memset(&req, 0, sizeof(req));
        strcpy(req.BrokerID, "9999");           // 模拟环境经纪商代码
        strcpy(req.UserID, "247060");           // 用户代码
        strcpy(req.Password, "RY20000219*");    // 密码
        
        int ret = m_pTraderApi->ReqUserLogin(&req, ++g_nRequestID);
        if (ret != 0) {
            std::cout << "交易API登录请求失败，错误代码: " << ret << std::endl;
        }
    }

    // 连接断开回调
    virtual void OnFrontDisconnected(int nReason) override
    {
        std::cout << "=== 交易API连接断开 ===" << std::endl;
        std::cout << "断开原因: " << nReason << std::endl;
        g_bTraderConnected = false;
        g_bTraderLoggedIn = false;
    }

    // 登录响应
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, 
                               CThostFtdcRspInfoField *pRspInfo, 
                               int nRequestID, bool bIsLast) override
    {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            std::cout << "=== 交易API登录失败 ===" << std::endl;
            std::cout << "错误代码: " << pRspInfo->ErrorID << std::endl;
            std::cout << "错误信息: " << pRspInfo->ErrorMsg << std::endl;
        } else {
            std::cout << "\n=== 交易API登录响应数据结构 ===" << std::endl;
            std::cout << "交易日: " << pRspUserLogin->TradingDay << std::endl;
            std::cout << "登录成功时间: " << pRspUserLogin->LoginTime << std::endl;
            std::cout << "经纪公司代码: " << pRspUserLogin->BrokerID << std::endl;
            std::cout << "用户代码: " << pRspUserLogin->UserID << std::endl;
            std::cout << "交易系统名称: " << pRspUserLogin->SystemName << std::endl;
            std::cout << "前置编号: " << pRspUserLogin->FrontID << std::endl;
            std::cout << "会话编号: " << pRspUserLogin->SessionID << std::endl;
            std::cout << "最大报单引用: " << pRspUserLogin->MaxOrderRef << std::endl;
            std::cout << "上期所时间: " << pRspUserLogin->SHFETime << std::endl;
            std::cout << "大商所时间: " << pRspUserLogin->DCETime << std::endl;
            std::cout << "郑商所时间: " << pRspUserLogin->CZCETime << std::endl;
            std::cout << "中金所时间: " << pRspUserLogin->FFEXTime << std::endl;
            std::cout << "能源中心时间: " << pRspUserLogin->INETime << std::endl;
            std::cout << "后台版本信息: " << pRspUserLogin->SysVersion << std::endl;
            std::cout << "广期所时间: " << pRspUserLogin->GFEXTime << std::endl;
            std::cout << "当前登录中心号: " << pRspUserLogin->LoginDRIdentityID << std::endl;
            std::cout << "用户所属中心号: " << pRspUserLogin->UserDRIdentityID << std::endl;
            std::cout << "----------------------------------------" << std::endl;
            g_bTraderLoggedIn = true;
            
            // 查询全部合约信息
            queryAllInstruments();
        }
    }

    // 查询全部合约信息
    void queryAllInstruments()
    {
        CThostFtdcQryInstrumentField req;
        memset(&req, 0, sizeof(req));
        
        int ret = m_pTraderApi->ReqQryInstrument(&req, ++g_nRequestID);
        if (ret == 0) {
            std::cout << "=== 请求全部合约信息成功 ===" << std::endl;
        } else {
            std::cout << "=== 请求全部合约信息失败，错误代码: " << ret << std::endl;
        }
    }

    // 查询合约信息响应
    virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, 
                                   CThostFtdcRspInfoField *pRspInfo, 
                                   int nRequestID, bool bIsLast) override
    {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            std::cout << "=== 查询合约信息失败 ===" << std::endl;
            std::cout << "错误代码: " << pRspInfo->ErrorID << std::endl;
            std::cout << "错误信息: " << pRspInfo->ErrorMsg << std::endl;
        } else if (pInstrument) {
            // 保存合约代码到全局列表
            g_instrumentList.push_back(std::string(pInstrument->InstrumentID));
            
            // 打印完整的合约信息数据结构
            // std::cout << "\n=== 合约信息数据结构 ===" << std::endl;
            // std::cout << "合约代码: " << pInstrument->InstrumentID << std::endl;
            // std::cout << "合约名称: " << pInstrument->InstrumentName << std::endl;
            // std::cout << "交易所: " << pInstrument->ExchangeID << std::endl;
            // std::cout << "产品代码: " << pInstrument->ProductID << std::endl;
            // std::cout << "产品类型: " << pInstrument->ProductClass << std::endl;
            // std::cout << "交割年份: " << pInstrument->DeliveryYear << std::endl;
            // std::cout << "交割月份: " << pInstrument->DeliveryMonth << std::endl;
            // std::cout << "市价单最大下单量: " << pInstrument->MaxMarketOrderVolume << std::endl;
            // std::cout << "市价单最小下单量: " << pInstrument->MinMarketOrderVolume << std::endl;
            // std::cout << "限价单最大下单量: " << pInstrument->MaxLimitOrderVolume << std::endl;
            // std::cout << "限价单最小下单量: " << pInstrument->MinLimitOrderVolume << std::endl;
            // std::cout << "合约数量乘数: " << pInstrument->VolumeMultiple << std::endl;
            // std::cout << "最小变动价位: " << pInstrument->PriceTick << std::endl;
            // std::cout << "创建日: " << pInstrument->CreateDate << std::endl;
            // std::cout << "上市日: " << pInstrument->OpenDate << std::endl;
            // std::cout << "到期日: " << pInstrument->ExpireDate << std::endl;
            // std::cout << "开始交割日: " << pInstrument->StartDelivDate << std::endl;
            // std::cout << "结束交割日: " << pInstrument->EndDelivDate << std::endl;
            // std::cout << "合约生命周期状态: " << pInstrument->InstLifePhase << std::endl;
            // std::cout << "当前是否交易: " << pInstrument->IsTrading << std::endl;
            // std::cout << "持仓类型: " << pInstrument->PositionType << std::endl;
            // std::cout << "持仓日期类型: " << pInstrument->PositionDateType << std::endl;
            // std::cout << "多头保证金率: " << pInstrument->LongMarginRatio << std::endl;
            // std::cout << "空头保证金率: " << pInstrument->ShortMarginRatio << std::endl;
            // std::cout << "是否使用大额单边保证金算法: " << pInstrument->MaxMarginSideAlgorithm << std::endl;
            // std::cout << "基础商品代码: " << pInstrument->UnderlyingInstrID << std::endl;
            // std::cout << "执行价: " << pInstrument->StrikePrice << std::endl;
            // std::cout << "期权类型: " << pInstrument->OptionsType << std::endl;
            // std::cout << "合约基础商品乘数: " << pInstrument->UnderlyingMultiple << std::endl;
            // std::cout << "组合类型: " << pInstrument->CombinationType << std::endl;
            // std::cout << "----------------------------------------" << std::endl;
        }
        
        if (bIsLast) {
            std::cout << "=== 全部合约信息查询完成，共获取 " << g_instrumentList.size() << " 个合约 ===" << std::endl;
            
            // 现在可以订阅这些合约的行情
            // subscribeAllInstruments();
        }
    }

    // 订阅所有获取到的合约行情
    void subscribeAllInstruments()
    {
        if (g_instrumentList.empty()) {
            std::cout << "=== 没有合约可订阅 ===" << std::endl;
            return;
        }

        // 转换为char*数组
        std::vector<char*> instrumentArray;
        for (const auto& instrument : g_instrumentList) {
            instrumentArray.push_back(const_cast<char*>(instrument.c_str()));
        }

        int ret = g_pMdApi->SubscribeMarketData(instrumentArray.data(), instrumentArray.size());
        if (ret == 0) {
            std::cout << "=== 订阅全部合约行情成功，共 " << instrumentArray.size() << " 个合约 ===" << std::endl;
        } else {
            std::cout << "=== 订阅全部合约行情失败，错误代码: " << ret << std::endl;
        }
    }
};

// 信号处理函数
void signalHandler(int signum)
{
    std::cout << "\n=== 收到退出信号，正在关闭... ===" << std::endl;
    if (g_pMdApi) {
        g_pMdApi->Release();
        g_pMdApi = nullptr;
    }
    if (g_pTraderApi) {
        g_pTraderApi->Release();
        g_pTraderApi = nullptr;
    }
    exit(signum);
}

int main()
{
    // 注册信号处理函数
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "=== CTP 行情客户端启动 ===" << std::endl;

    // 创建行情API实例
    g_pMdApi = CThostFtdcMdApi::CreateFtdcMdApi("./", false, false);
    if (!g_pMdApi) {
        std::cout << "创建行情API实例失败" << std::endl;
        return -1;
    }

    // 创建交易API实例
    // g_pTraderApi = CThostFtdcTraderApi::CreateFtdcTraderApi("./");
    // if (!g_pTraderApi) {
    //     std::cout << "创建交易API实例失败" << std::endl;
    //     return -1;
    // }

    // 创建回调实例
    CMdSpi mdSpi(g_pMdApi);
    // CTraderSpi traderSpi(g_pTraderApi);
    
    g_pMdApi->RegisterSpi(&mdSpi);
    // g_pTraderApi->RegisterSpi(&traderSpi)

    // 注册前置地址
    g_pMdApi->RegisterFront((char*)"tcp://101.231.162.58:41213");  // 模拟环境行情地址
    // g_pTraderApi->RegisterFront((char*)"tcp://182.254.243.31:30001");  // 模拟环境交易地址

    // 初始化API
    g_pMdApi->Init();
    // g_pTraderApi->Init();

    std::cout << "正在连接服务器..." << std::endl;

    // 主循环
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (!g_bConnected) {
            std::cout << "等待连接..." << std::endl;
            continue;
        }
        
        if (!g_bLoggedIn) {
            std::cout << "等待登录..." << std::endl;
            continue;
        }
        
        // 连接成功且已登录，等待行情数据
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
