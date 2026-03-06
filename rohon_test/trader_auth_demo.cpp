// @yutiansut @quantaxis
// 融航柜台 CTP 接口测试：认证 + 登录 + 结算确认 + 下单 + 撤单

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <mutex>
#include <atomic>
#include <iomanip>
#include <iconv.h>
#include "ThostFtdcTraderApi.h"

// ==================== 编码转换 ====================
// CTP/融航所有字符串字段均为 GBK，终端为 UTF-8，需要转换后打印。

static std::string gbk2utf8(const char* gbk)
{
    if (!gbk || gbk[0] == '\0') return "";
    iconv_t cd = iconv_open("UTF-8", "GBK");
    if (cd == (iconv_t)-1) return std::string(gbk);

    size_t inbytesleft  = strlen(gbk);
    size_t outbytesleft = inbytesleft * 3;
    std::string buf(outbytesleft, '\0');
    char* inptr  = const_cast<char*>(gbk);
    char* outptr = &buf[0];

    iconv(cd, &inptr, &inbytesleft, &outptr, &outbytesleft);
    iconv_close(cd);
    buf.resize(buf.size() - outbytesleft);
    return buf;
}

// ==================== Config ====================

bool parseJsonConfig(const std::string& filename,
                     std::string& frontAddress,
                     std::string& brokerID,
                     std::string& userID,
                     std::string& password,
                     std::string& appID,
                     std::string& authCode,
                     std::string& userProductInfo)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "错误: 无法打开配置文件: " << filename << std::endl;
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    auto getValue = [&content](const std::string& key) -> std::string {
        std::string searchKey = "\"" + key + "\"";
        size_t pos = content.find(searchKey);
        if (pos == std::string::npos) return "";
        pos = content.find(":", pos);
        if (pos == std::string::npos) return "";
        pos++;
        while (pos < content.length() && (content[pos] == ' ' || content[pos] == '\t')) pos++;
        if (pos >= content.length()) return "";
        if (content[pos] == '"') {
            pos++;
            size_t end = content.find('"', pos);
            if (end == std::string::npos) return "";
            return content.substr(pos, end - pos);
        } else {
            size_t end = pos;
            while (end < content.length() &&
                   content[end] != ',' && content[end] != '}' &&
                   content[end] != '\n' && content[end] != '\r') {
                end++;
            }
            std::string value = content.substr(pos, end - pos);
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
                value.pop_back();
            return value;
        }
    };

    frontAddress    = getValue("front_address");
    brokerID        = getValue("broker_id");
    userID          = getValue("user_id");
    password        = getValue("password");
    appID           = getValue("app_id");
    authCode        = getValue("auth_code");
    userProductInfo = getValue("user_product_info");

    if (frontAddress.empty() || brokerID.empty() || userID.empty() || password.empty()) {
        std::cerr << "错误: 配置文件缺少必要字段" << std::endl;
        return false;
    }
    return true;
}

// ==================== Global State ====================

CThostFtdcTraderApi* g_pTraderApi = nullptr;
std::atomic<int>  g_nRequestID{0};
std::atomic<int>  g_nOrderActionRef{0};
std::atomic<int>  g_nOrderRef{1};
int               g_FrontID   = 0;
int               g_SessionID = 0;

std::string g_FrontAddress, g_BrokerID, g_UserID, g_Password;
std::string g_AppID, g_AuthCode, g_UserProductInfo;

std::atomic<bool> g_bLoggedIn{false};
std::atomic<bool> g_bReady{false};   // settlement confirmed → ready to trade
std::atomic<bool> g_bShouldExit{false};

// ==================== Order Tracking ====================

struct OrderInfo {
    std::string orderRef;
    std::string exchangeID;
    std::string instrumentID;
    std::string orderSysID;
    std::string direction;
    std::string status;
    double      price  = 0.0;
    int         volume = 0;
};

std::map<std::string, OrderInfo> g_orders;
std::mutex g_orderMutex;

// ==================== Helpers ====================

static void printHelp()
{
    std::cout <<
        "\n可用命令:\n"
        "  order B|S <EXCHANGE> <INSTRUMENT> <PRICE> <VOL> <open|close|closetoday|closeyesterday>\n"
        "      例: order B SHFE rb2501 3000.0 1 open\n"
        "          order S SHFE rb2501 3001.0 1 close\n"
        "  cancel <OrderRef>     -- 按本地 OrderRef 撤单\n"
        "  list                  -- 列出当日所有报单\n"
        "  help                  -- 显示此帮助\n"
        "  quit                  -- 退出\n"
        << std::endl;
}

static void printOrderList()
{
    std::lock_guard<std::mutex> lk(g_orderMutex);
    if (g_orders.empty()) {
        std::cout << "(暂无报单)" << std::endl;
        return;
    }
    std::cout << std::left
              << std::setw(6)  << "Ref"
              << std::setw(10) << "合约"
              << std::setw(6)  << "方向"
              << std::setw(10) << "价格"
              << std::setw(6)  << "数量"
              << std::setw(20) << "SysID"
              << "状态\n"
              << std::string(70, '-') << std::endl;
    for (auto& kv : g_orders) {
        const auto& o = kv.second;
        std::cout << std::left
                  << std::setw(6)  << o.orderRef
                  << std::setw(10) << o.instrumentID
                  << std::setw(6)  << o.direction
                  << std::setw(10) << o.price
                  << std::setw(6)  << o.volume
                  << std::setw(20) << o.orderSysID
                  << o.status << "\n";
    }
    std::cout << std::endl;
}

// ==================== SPI ====================

class CTraderSpi : public CThostFtdcTraderSpi
{
    CThostFtdcTraderApi* m_api;

public:
    explicit CTraderSpi(CThostFtdcTraderApi* api) : m_api(api) {}

    // ---------- 连接 ----------

    void OnFrontConnected() override
    {
        std::cout << "\n[连接] 融航柜台连接成功，开始认证..." << std::endl;
        reqAuthenticate();
    }

    void OnFrontDisconnected(int nReason) override
    {
        std::cout << "[断开] 连接断开，原因: " << nReason << std::endl;
        g_bLoggedIn = false;
        g_bReady    = false;
    }

    void OnHeartBeatWarning(int nTimeLapse) override
    {
        std::cout << "[心跳] 警告，已 " << nTimeLapse << "s 未收到数据" << std::endl;
    }

    // ---------- 认证 ----------

    void reqAuthenticate()
    {
        CThostFtdcReqAuthenticateField req = {};
        strncpy(req.BrokerID, g_BrokerID.c_str(), sizeof(req.BrokerID) - 1);
        strncpy(req.UserID,   g_UserID.c_str(),   sizeof(req.UserID)   - 1);
        strncpy(req.AppID,    g_AppID.c_str(),    sizeof(req.AppID)    - 1);
        strncpy(req.AuthCode, g_AuthCode.c_str(), sizeof(req.AuthCode) - 1);
        if (!g_UserProductInfo.empty())
            strncpy(req.UserProductInfo, g_UserProductInfo.c_str(), sizeof(req.UserProductInfo) - 1);
        m_api->ReqAuthenticate(&req, ++g_nRequestID);
    }

    void OnRspAuthenticate(CThostFtdcRspAuthenticateField*, CThostFtdcRspInfoField* i,
                           int, bool) override
    {
        if (i && i->ErrorID != 0) {
            std::cerr << "[认证] 失败: [" << i->ErrorID << "] " << gbk2utf8(i->ErrorMsg) << std::endl;
            return;
        }
        std::cout << "[认证] 成功，发送登录..." << std::endl;
        reqUserLogin();
    }

    // ---------- 登录 ----------

    void reqUserLogin()
    {
        CThostFtdcReqUserLoginField req = {};
        strncpy(req.BrokerID, g_BrokerID.c_str(), sizeof(req.BrokerID) - 1);
        strncpy(req.UserID,   g_UserID.c_str(),   sizeof(req.UserID)   - 1);
        strncpy(req.Password, g_Password.c_str(), sizeof(req.Password) - 1);
        m_api->ReqUserLogin(&req, ++g_nRequestID);
    }

    void OnRspUserLogin(CThostFtdcRspUserLoginField* f, CThostFtdcRspInfoField* i,
                        int, bool) override
    {
        if (i && i->ErrorID != 0) {
            std::cerr << "[登录] 失败: [" << i->ErrorID << "] " << gbk2utf8(i->ErrorMsg) << std::endl;
            return;
        }
        g_FrontID   = f->FrontID;
        g_SessionID = f->SessionID;
        // MaxOrderRef 是本 session 已用过的最大 OrderRef，新单从 +1 开始
        g_nOrderRef = atoi(f->MaxOrderRef) + 1;

        std::cout << "[登录] 成功\n"
                  << "  交易日:      " << f->TradingDay    << "\n"
                  << "  FrontID:     " << g_FrontID        << "\n"
                  << "  SessionID:   " << g_SessionID      << "\n"
                  << "  MaxOrderRef: " << f->MaxOrderRef   << "  下一单号: " << g_nOrderRef.load()
                  << std::endl;

        g_bLoggedIn = true;
        reqSettlementConfirm();
    }

    void OnRspUserLogout(CThostFtdcUserLogoutField*, CThostFtdcRspInfoField*,
                         int, bool) override
    {
        std::cout << "[登出] 成功" << std::endl;
        g_bLoggedIn = false;
        g_bReady    = false;
    }

    // ---------- 结算确认 ----------

    void reqSettlementConfirm()
    {
        std::cout << "[结算] 发送结算确认..." << std::endl;
        CThostFtdcSettlementInfoConfirmField req = {};
        strncpy(req.BrokerID,   g_BrokerID.c_str(), sizeof(req.BrokerID)   - 1);
        strncpy(req.InvestorID, g_UserID.c_str(),   sizeof(req.InvestorID) - 1);
        m_api->ReqSettlementInfoConfirm(&req, ++g_nRequestID);
    }

    void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField*,
                                    CThostFtdcRspInfoField* i, int, bool) override
    {
        if (i && i->ErrorID != 0) {
            // 部分柜台不要求结算确认，忽略错误仍可交易
            std::cout << "[结算] 跳过确认 [" << i->ErrorID << "] " << gbk2utf8(i->ErrorMsg) << std::endl;
        } else {
            std::cout << "[结算] 确认成功" << std::endl;
        }
        g_bReady = true;
        std::cout << "\n========== 就绪，可以下单 ==========" << std::endl;
        printHelp();
    }

    // ==================== 下单 ====================

    void reqInsertOrder(const std::string& exchange,
                        const std::string& instrument,
                        char direction,
                        char offset,
                        double price,
                        int volume)
    {
        int orderRef = g_nOrderRef++;

        CThostFtdcInputOrderField req = {};
        strncpy(req.BrokerID,    g_BrokerID.c_str(),  sizeof(req.BrokerID)    - 1);
        strncpy(req.InvestorID,  g_UserID.c_str(),    sizeof(req.InvestorID)  - 1);
        strncpy(req.ExchangeID,  exchange.c_str(),    sizeof(req.ExchangeID)  - 1);
        strncpy(req.InstrumentID,instrument.c_str(),  sizeof(req.InstrumentID)- 1);
        snprintf(req.OrderRef, sizeof(req.OrderRef), "%d", orderRef);

        req.OrderPriceType       = THOST_FTDC_OPT_LimitPrice;
        req.Direction            = direction;
        req.CombOffsetFlag[0]    = offset;
        req.CombHedgeFlag[0]     = THOST_FTDC_HF_Speculation;
        req.LimitPrice           = price;
        req.VolumeTotalOriginal  = volume;
        req.TimeCondition        = THOST_FTDC_TC_GFD;
        req.VolumeCondition      = THOST_FTDC_VC_AV;
        req.ContingentCondition  = THOST_FTDC_CC_Immediately;
        req.MinVolume            = 1;
        req.ForceCloseReason     = THOST_FTDC_FCC_NotForceClose;
        req.IsAutoSuspend        = 0;
        req.UserForceClose       = 0;

        {
            std::lock_guard<std::mutex> lk(g_orderMutex);
            OrderInfo info;
            info.orderRef     = std::to_string(orderRef);
            info.exchangeID   = exchange;
            info.instrumentID = instrument;
            info.direction    = (direction == THOST_FTDC_D_Buy) ? "BUY" : "SELL";
            info.price        = price;
            info.volume       = volume;
            info.status       = "已报";
            g_orders[info.orderRef] = info;
        }

        int ret = m_api->ReqOrderInsert(&req, ++g_nRequestID);
        std::cout << "[下单] OrderRef=" << orderRef
                  << "  " << exchange << "." << instrument
                  << "  " << (direction == THOST_FTDC_D_Buy ? "BUY" : "SELL")
                  << "  价=" << price << "  量=" << volume
                  << "  offset=" << offset
                  << "  ret=" << ret << std::endl;
    }

    // ==================== 撤单 ====================

    // 按本地 OrderRef 撤单（同一 session 内最可靠）
    void reqCancelOrder(const std::string& orderRef)
    {
        std::string exchID, instID;
        {
            std::lock_guard<std::mutex> lk(g_orderMutex);
            auto it = g_orders.find(orderRef);
            if (it == g_orders.end()) {
                std::cerr << "[撤单] 未找到 OrderRef=" << orderRef
                          << "，请用 list 命令确认报单列表" << std::endl;
                return;
            }
            exchID = it->second.exchangeID;
            instID = it->second.instrumentID;
        }

        CThostFtdcInputOrderActionField req = {};
        strncpy(req.BrokerID,    g_BrokerID.c_str(), sizeof(req.BrokerID)    - 1);
        strncpy(req.InvestorID,  g_UserID.c_str(),   sizeof(req.InvestorID)  - 1);
        strncpy(req.ExchangeID,  exchID.c_str(),     sizeof(req.ExchangeID)  - 1);
        strncpy(req.InstrumentID,instID.c_str(),     sizeof(req.InstrumentID)- 1);
        strncpy(req.OrderRef,    orderRef.c_str(),   sizeof(req.OrderRef)    - 1);
        req.OrderActionRef = ++g_nOrderActionRef;
        req.FrontID        = g_FrontID;
        req.SessionID      = g_SessionID;
        req.ActionFlag     = THOST_FTDC_AF_Delete;

        int ret = m_api->ReqOrderAction(&req, ++g_nRequestID);
        std::cout << "[撤单] OrderRef=" << orderRef << "  ret=" << ret << std::endl;
    }

    // ==================== 报单回调 ====================

    // 柜台同步拒绝（字段校验失败等）
    void OnRspOrderInsert(CThostFtdcInputOrderField* f, CThostFtdcRspInfoField* i,
                          int, bool) override
    {
        if (i && i->ErrorID != 0) {
            std::cerr << "[报单拒绝] OrderRef=" << (f ? f->OrderRef : "?")
                      << "  [" << i->ErrorID << "] " << gbk2utf8(i->ErrorMsg) << std::endl;
            if (f) {
                std::lock_guard<std::mutex> lk(g_orderMutex);
                auto it = g_orders.find(std::string(f->OrderRef));
                if (it != g_orders.end())
                    it->second.status = "拒绝:" + std::string(gbk2utf8(i->ErrorMsg));
            }
        }
    }

    // 交易所异步拒绝
    void OnErrRtnOrderInsert(CThostFtdcInputOrderField* f,
                             CThostFtdcRspInfoField* i) override
    {
        if (i && i->ErrorID != 0) {
            std::cerr << "[下单错误] OrderRef=" << (f ? f->OrderRef : "?")
                      << "  [" << i->ErrorID << "] " << gbk2utf8(i->ErrorMsg) << std::endl;
        }
    }

    // 撤单请求拒绝
    void OnRspOrderAction(CThostFtdcInputOrderActionField*, CThostFtdcRspInfoField* i,
                          int, bool) override
    {
        if (i && i->ErrorID != 0) {
            std::cerr << "[撤单拒绝] [" << i->ErrorID << "] " << gbk2utf8(i->ErrorMsg) << std::endl;
        }
    }

    // 交易所异步拒绝撤单
    void OnErrRtnOrderAction(CThostFtdcOrderActionField*,
                             CThostFtdcRspInfoField* i) override
    {
        if (i && i->ErrorID != 0) {
            std::cerr << "[撤单错误] [" << i->ErrorID << "] " << gbk2utf8(i->ErrorMsg) << std::endl;
        }
    }

    // 报单状态推送（报、撤、拒、全成等）
    void OnRtnOrder(CThostFtdcOrderField* f) override
    {
        if (!f) return;

        // OrderRef 末尾可能有空格，trim 一下
        std::string orderRef = f->OrderRef;
        size_t last = orderRef.find_last_not_of(' ');
        if (last != std::string::npos) orderRef = orderRef.substr(0, last + 1);

        {
            std::lock_guard<std::mutex> lk(g_orderMutex);
            auto it = g_orders.find(orderRef);
            if (it != g_orders.end()) {
                it->second.orderSysID = f->OrderSysID;
                it->second.status     = gbk2utf8(f->StatusMsg);
            }
        }

        std::cout << "[报单推送] OrderRef=" << orderRef
                  << "  SysID="   << f->OrderSysID
                  << "  " << f->InstrumentID
                  << "  剩余=" << f->VolumeTotal
                  << "  状态=" << gbk2utf8(f->StatusMsg)
                  << std::endl;
    }

    // 成交推送
    void OnRtnTrade(CThostFtdcTradeField* f) override
    {
        if (!f) return;
        std::cout << "[成交推送] OrderRef=" << f->OrderRef
                  << "  " << f->ExchangeID << "." << f->InstrumentID
                  << "  " << (f->Direction == THOST_FTDC_D_Buy ? "BUY" : "SELL")
                  << "  价=" << f->Price
                  << "  量=" << f->Volume
                  << "  时间=" << f->TradeTime
                  << std::endl;
    }

    // 通用错误
    void OnRspError(CThostFtdcRspInfoField* i, int reqId, bool) override
    {
        if (i && i->ErrorID != 0)
            std::cerr << "[错误] ReqID=" << reqId
                      << "  [" << i->ErrorID << "] " << gbk2utf8(i->ErrorMsg) << std::endl;
    }
};

// ==================== 交互命令循环 ====================

static CTraderSpi* g_pSpi = nullptr;

static void commandLoop()
{
    // 等待登录 + 结算确认完成
    while (!g_bReady && !g_bShouldExit)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string line;
    while (!g_bShouldExit) {
        std::cout << "> ";
        std::cout.flush();

        if (!std::getline(std::cin, line)) break;

        // 去首尾空白
        size_t s = line.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        line = line.substr(s);

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "quit" || cmd == "exit") {
            g_bShouldExit = true;
            break;

        } else if (cmd == "order") {
            // order B|S EXCHANGE INSTRUMENT PRICE VOL open|close|closetoday|closeyesterday
            std::string dirStr, exchange, instrument, offsetStr;
            double price;
            int    volume;
            if (!(iss >> dirStr >> exchange >> instrument >> price >> volume >> offsetStr)) {
                std::cerr << "用法: order B|S EXCHANGE INSTRUMENT PRICE VOL open|close|closetoday|closeyesterday"
                          << std::endl;
                continue;
            }
            if (!g_bReady) { std::cerr << "尚未就绪，请等待登录完成" << std::endl; continue; }

            char dir = (dirStr == "B" || dirStr == "b") ? THOST_FTDC_D_Buy : THOST_FTDC_D_Sell;

            char offset;
            if      (offsetStr == "open")            offset = THOST_FTDC_OF_Open;
            else if (offsetStr == "closetoday")      offset = THOST_FTDC_OF_CloseToday;
            else if (offsetStr == "closeyesterday")  offset = THOST_FTDC_OF_CloseYesterday;
            else                                     offset = THOST_FTDC_OF_Close;

            g_pSpi->reqInsertOrder(exchange, instrument, dir, offset, price, volume);

        } else if (cmd == "cancel") {
            std::string orderRef;
            if (!(iss >> orderRef)) {
                std::cerr << "用法: cancel <OrderRef>" << std::endl;
                continue;
            }
            if (!g_bReady) { std::cerr << "尚未就绪" << std::endl; continue; }
            g_pSpi->reqCancelOrder(orderRef);

        } else if (cmd == "list") {
            printOrderList();

        } else if (cmd == "help") {
            printHelp();

        } else {
            std::cerr << "未知命令: " << cmd << std::endl;
            printHelp();
        }
    }
}

// ==================== Signal ====================

// 注意：从信号处理器中调用 Release() 不安全（API 内部线程可能正在执行回调，会死锁）。
// libLinuxDataCollect.so 在获取不到硬件权限时会向进程自发 SIGTERM，
// 此处仅用 write() + _exit() 保证 async-signal-safe。
static volatile sig_atomic_t g_signalCount = 0;

static void signalHandler(int signum)
{
    if (g_signalCount++ > 0) _exit(signum);
    const char msg[] = "\n[退出]\n";
    if (write(STDOUT_FILENO, msg, sizeof(msg) - 1)) {}
    g_bShouldExit = true;
    _exit(0);
}

// ==================== Main ====================

int main(int argc, char* argv[])
{
    signal(SIGINT,  signalHandler);
    // SIGTERM 可能来自 libLinuxDataCollect.so 的硬件权限检测，忽略它；
    // Ctrl+C (SIGINT) 仍然可以正常退出。
    signal(SIGTERM, SIG_IGN);

    std::string configFile = (argc > 1) ? argv[1] : "config.json";

    std::cout << "========================================\n"
              << "  融航柜台 CTP 下单/撤单 Demo\n"
              << "========================================\n"
              << "配置文件: " << configFile << std::endl;

    if (!parseJsonConfig(configFile,
                         g_FrontAddress, g_BrokerID, g_UserID,
                         g_Password, g_AppID, g_AuthCode, g_UserProductInfo)) {
        std::cerr << "用法: " << argv[0] << " [config.json]" << std::endl;
        return -1;
    }

    std::cout << "前置地址: " << g_FrontAddress << "\n"
              << "BrokerID: " << g_BrokerID     << "\n"
              << "UserID:   " << g_UserID        << "\n"
              << "AppID:    " << g_AppID         << std::endl;

    g_pTraderApi = CThostFtdcTraderApi::CreateFtdcTraderApi("./flow/");
    if (!g_pTraderApi) {
        std::cerr << "创建 TraderApi 失败" << std::endl;
        return -1;
    }

    CTraderSpi spi(g_pTraderApi);
    g_pSpi = &spi;

    g_pTraderApi->RegisterSpi(&spi);
    g_pTraderApi->RegisterFront((char*)g_FrontAddress.c_str());
    g_pTraderApi->Init();

    std::cout << "正在连接服务器..." << std::endl;

    // 命令循环在独立线程中运行，主线程等待其结束
    std::thread cmdThread(commandLoop);
    cmdThread.join();

    // 清理
    if (g_pTraderApi) {
        g_pTraderApi->RegisterSpi(nullptr);
        g_pTraderApi->Release();
        g_pTraderApi = nullptr;
    }
    std::cout << "程序退出" << std::endl;
    return 0;
}
