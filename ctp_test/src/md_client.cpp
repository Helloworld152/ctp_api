#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <signal.h>

#include "ThostFtdcMdApi.h"

static CThostFtdcMdApi* g_pMdApi = nullptr;
static bool g_bLoggedIn = false;
static int g_nRequestID = 0;

struct MdConfig {
    std::string front_address;
    std::string broker_id;
    std::string user_id;
    std::string password;
    std::vector<std::string> instruments;
};

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end == std::string::npos ? std::string::npos : end - start + 1);
}

static bool loadMdConfig(const std::string& path, MdConfig& cfg) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line, section;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (section == "MD") {
            if (key == "FrontAddress") cfg.front_address = val;
            else if (key == "BrokerID") cfg.broker_id = val;
            else if (key == "UserID") cfg.user_id = val;
            else if (key == "Password") cfg.password = val;
        } else if (section == "INSTRUMENTS" && key == "Instruments") {
            cfg.instruments.clear();
            for (size_t i = 0; i < val.size(); ) {
                size_t j = val.find(',', i);
                if (j == std::string::npos) j = val.size();
                std::string id = trim(val.substr(i, j - i));
                if (!id.empty()) cfg.instruments.push_back(id);
                i = j + 1;
            }
        }
    }
    return !cfg.front_address.empty() && !cfg.broker_id.empty() && !cfg.user_id.empty() && !cfg.password.empty();
}

class CMdSpi : public CThostFtdcMdSpi {
    CThostFtdcMdApi* m_pMdApi;
    MdConfig m_cfg;

public:
    CMdSpi(CThostFtdcMdApi* api, const MdConfig& cfg) : m_pMdApi(api), m_cfg(cfg) {}

    void OnFrontConnected() override {
        std::cout << "[MD] Front connected" << std::endl;
        reqUserLogin();
    }

    void OnFrontDisconnected(int nReason) override {
        std::cout << "[MD] Front disconnected, reason=" << nReason << std::endl;
        g_bLoggedIn = false;
    }

    void OnHeartBeatWarning(int nTimeLapse) override {
        std::cout << "[MD] HeartBeat warning " << nTimeLapse << "s" << std::endl;
    }

    void OnRspUserLogin(CThostFtdcRspUserLoginField* pRsp, CThostFtdcRspInfoField* pRspInfo,
                        int nRequestID, bool bIsLast) override {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            std::cerr << "[MD] Login failed ErrorID=" << pRspInfo->ErrorID << " " << pRspInfo->ErrorMsg << std::endl;
            return;
        }
        std::cout << "[MD] Login ok, TradingDay=" << (pRsp ? pRsp->TradingDay : "") << std::endl;
        g_bLoggedIn = true;
        subscribeMarketData();
    }

    void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* p) override {
        if (!p) return;
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::time_t t = ms / 1000;
        std::tm* tm = std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(tm, "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << (ms % 1000);
        std::cout << "[" << oss.str() << "] " << p->InstrumentID << " " << p->LastPrice
                  << " " << p->UpdateTime << "." << p->UpdateMillisec << "\n"
                  << "  买五档: "
                  << p->BidPrice1 << "x" << p->BidVolume1 << " "
                  << p->BidPrice2 << "x" << p->BidVolume2 << " "
                  << p->BidPrice3 << "x" << p->BidVolume3 << " "
                  << p->BidPrice4 << "x" << p->BidVolume4 << " "
                  << p->BidPrice5 << "x" << p->BidVolume5 << "\n"
                  << "  卖五档: "
                  << p->AskPrice1 << "x" << p->AskVolume1 << " "
                  << p->AskPrice2 << "x" << p->AskVolume2 << " "
                  << p->AskPrice3 << "x" << p->AskVolume3 << " "
                  << p->AskPrice4 << "x" << p->AskVolume4 << " "
                  << p->AskPrice5 << "x" << p->AskVolume5 << std::endl;
    }

    void OnRspSubMarketData(CThostFtdcSpecificInstrumentField* pInst, CThostFtdcRspInfoField* pRspInfo,
                           int nRequestID, bool bIsLast) override {
        if (pRspInfo && pRspInfo->ErrorID != 0)
            std::cerr << "[MD] Sub failed " << (pInst ? pInst->InstrumentID : "") << " " << pRspInfo->ErrorMsg << std::endl;
        else if (pInst)
            std::cout << "[MD] Sub ok " << pInst->InstrumentID << std::endl;
    }

private:
    void reqUserLogin() {
        CThostFtdcReqUserLoginField req;
        memset(&req, 0, sizeof(req));
        strncpy(req.BrokerID, m_cfg.broker_id.c_str(), sizeof(req.BrokerID) - 1);
        strncpy(req.UserID, m_cfg.user_id.c_str(), sizeof(req.UserID) - 1);
        strncpy(req.Password, m_cfg.password.c_str(), sizeof(req.Password) - 1);
        int ret = m_pMdApi->ReqUserLogin(&req, ++g_nRequestID);
        if (ret != 0) std::cerr << "[MD] ReqUserLogin ret=" << ret << std::endl;
    }

    void subscribeMarketData() {
        if (m_cfg.instruments.empty()) {
            std::cout << "[MD] No instruments in config, skip subscribe" << std::endl;
            return;
        }
        std::vector<char*> ptrs;
        for (auto& s : m_cfg.instruments) ptrs.push_back(const_cast<char*>(s.c_str()));
        int ret = m_pMdApi->SubscribeMarketData(ptrs.data(), ptrs.size());
        if (ret != 0) std::cerr << "[MD] SubscribeMarketData ret=" << ret << std::endl;
    }
};

static void signalHandler(int) {
    if (g_pMdApi) { g_pMdApi->Release(); g_pMdApi = nullptr; }
    exit(0);
}

int main(int argc, char* argv[]) {
    std::string config_path = "config/config.ini";
    if (argc > 1) config_path = argv[1];

    MdConfig cfg;
    if (!loadMdConfig(config_path, cfg)) {
        std::cerr << "Load config failed: " << config_path << " (need [MD] FrontAddress,BrokerID,UserID,Password)" << std::endl;
        return 1;
    }
    if (cfg.instruments.empty())
        std::cout << "No [INSTRUMENTS] Instruments= in config, will not subscribe." << std::endl;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    g_pMdApi = CThostFtdcMdApi::CreateFtdcMdApi("flow_md/", false, false);
    if (!g_pMdApi) {
        std::cerr << "CreateFtdcMdApi failed" << std::endl;
        return 1;
    }

    CMdSpi spi(g_pMdApi, cfg);
    g_pMdApi->RegisterSpi(&spi);
    g_pMdApi->RegisterFront(const_cast<char*>(cfg.front_address.c_str()));
    g_pMdApi->Init();

    std::cout << "[MD] Connecting to " << cfg.front_address << " ..." << std::endl;
    while (true)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
