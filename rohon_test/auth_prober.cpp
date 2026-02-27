#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>
#include <vector>
#include "ThostFtdcTraderApi.h"

struct Config {
    std::vector<std::string> fronts;
    std::string broker, user, pass, appid, auth;
};

bool loadConfig(const std::string& path, Config& c) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    
    auto getV = [&](const std::string& key) {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return std::string("");
        pos = content.find(":", pos);
        if (pos == std::string::npos) return std::string("");
        pos++;
        
        // 跳过空白
        while (pos < content.length() && (content[pos] == ' ' || content[pos] == '\t' || content[pos] == '\n' || content[pos] == '\r')) pos++;
        
        if (pos < content.length() && content[pos] == '"') {
            pos++; // 跳过起始引号
            size_t end = content.find('"', pos); // 找到结束引号，不被中间的逗号迷惑
            if (end == std::string::npos) return std::string("");
            return content.substr(pos, end - pos);
        } else {
            size_t end = content.find_first_of(",}", pos);
            if (end == std::string::npos) return std::string("");
            return content.substr(pos, end - pos);
        }
    };
    
    std::string addrs = getV("front_address");
    std::stringstream ss(addrs);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
            // 去除可能存在的空白
            size_t first = item.find_first_not_of(" \t\r\n");
            size_t last = item.find_last_not_of(" \t\r\n");
            if (first != std::string::npos)
                c.fronts.push_back(item.substr(first, (last - first + 1)));
        }
    }
    
    c.broker = getV("broker_id");
    c.user = getV("user_id");
    c.pass = getV("password");
    c.appid = getV("app_id");
    c.auth = getV("auth_code");
    return !c.fronts.empty();
}

std::string now() {
    auto now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

class ProbeSpi : public CThostFtdcTraderSpi {
public:
    CThostFtdcTraderApi* api;
    Config cfg;
    std::atomic<int> status{0}; 
    std::string result_msg = "TIMEOUT (No Front Connected)";

    ProbeSpi(CThostFtdcTraderApi* a, const Config& c) : api(a), cfg(c) {}

    void OnFrontConnected() override {
        status = 1;
        result_msg = "CONNECTED (Waiting Auth)";
        CThostFtdcReqAuthenticateField req = {};
        strncpy(req.BrokerID, cfg.broker.c_str(), sizeof(req.BrokerID)-1);
        strncpy(req.UserID, cfg.user.c_str(), sizeof(req.UserID)-1);
        strncpy(req.AppID, cfg.appid.c_str(), sizeof(req.AppID)-1);
        strncpy(req.AuthCode, cfg.auth.c_str(), sizeof(req.AuthCode)-1);
        api->ReqAuthenticate(&req, 1);
    }

    void OnRspAuthenticate(CThostFtdcRspAuthenticateField *f, CThostFtdcRspInfoField *i, int r, bool last) override {
        if (i && i->ErrorID != 0) {
            std::string msg = i->ErrorMsg;
            for (char &ch : msg) if (ch == '\n' || ch == '\r') ch = ' ';
            result_msg = "INVALID: [" + std::to_string(i->ErrorID) + "] " + msg;
        } else {
            result_msg = "VALID (Auth Success)";
        }
        status = 2;
    }

    void OnFrontDisconnected(int nReason) override {
        if (status < 2) {
            result_msg = "DISCONNECTED: " + std::to_string(nReason);
        }
    }
};

void run_probe(const Config& cfg) {
    CThostFtdcTraderApi* api = CThostFtdcTraderApi::CreateFtdcTraderApi("./flow_probe/");
    ProbeSpi spi(api, cfg);
    api->RegisterSpi(&spi);
    for (const auto& addr : cfg.fronts) {
        api->RegisterFront((char*)addr.c_str());
    }
    api->Init();

    for (int i = 0; i < 100; ++i) {
        if (spi.status == 2) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[" << now() << "] Probe Result: " << spi.result_msg << std::endl;

    api->RegisterSpi(nullptr);
    api->Release();
}

int main() {
    Config cfg;
    if (!loadConfig("config.json", cfg)) {
        std::cerr << "Config load failed!" << std::endl;
        return 1;
    }

    std::cout << "=== Rohon Auth Prober Native Mode ===" << std::endl;
    std::cout << "Detected Fronts: " << cfg.fronts.size() << std::endl;
    for(const auto& f : cfg.fronts) std::cout << "  -> " << f << std::endl;
    
    while (true) {
        auto next_run = std::chrono::system_clock::now() + std::chrono::minutes(1);
        run_probe(cfg);
        std::this_thread::sleep_until(next_run);
    }
    return 0;
}