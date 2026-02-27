#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <cstring>
#include <fstream>
#include <string>
#include "ThostFtdcTraderApi.h"

// JSON解析简单实现（如果不想引入外部库，可以用简单的方式）
// 这里使用简单的字符串解析，实际项目中建议使用json库
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
    
    // 简单的JSON解析（移除空白和引号）
    auto getValue = [&content](const std::string& key) -> std::string {
        std::string searchKey = "\"" + key + "\"";
        size_t pos = content.find(searchKey);
        if (pos == std::string::npos) return "";
        
        pos = content.find(":", pos);
        if (pos == std::string::npos) return "";
        pos++; // 跳过冒号
        
        // 跳过空白
        while (pos < content.length() && (content[pos] == ' ' || content[pos] == '\t')) pos++;
        
        if (pos >= content.length()) return "";
        
        // 读取值（可能是字符串或数字）
        if (content[pos] == '"') {
            // 字符串值
            pos++; // 跳过引号
            size_t end = content.find('"', pos);
            if (end == std::string::npos) return "";
            return content.substr(pos, end - pos);
        } else {
            // 数字或其他值（读取到逗号或}）
            size_t end = pos;
            while (end < content.length() && 
                   content[end] != ',' && 
                   content[end] != '}' && 
                   content[end] != '\n' &&
                   content[end] != '\r') {
                end++;
            }
            std::string value = content.substr(pos, end - pos);
            // 去除尾部空白
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
                value.pop_back();
            }
            return value;
        }
    };
    
    frontAddress = getValue("front_address");
    brokerID = getValue("broker_id");
    userID = getValue("user_id");
    password = getValue("password");
    appID = getValue("app_id");
    authCode = getValue("auth_code");
    userProductInfo = getValue("user_product_info");
    
    if (frontAddress.empty() || brokerID.empty() || userID.empty() || password.empty()) {
        std::cerr << "错误: 配置文件缺少必要字段" << std::endl;
        return false;
    }
    
    return true;
}

// 全局变量
CThostFtdcTraderApi* g_pTraderApi = nullptr;
bool g_bTraderConnected = false;
bool g_bTraderAuthenticated = false;
bool g_bTraderLoggedIn = false;
bool g_bShouldExit = false;  // 退出标志
int g_nRequestID = 0;

// 认证信息（从配置文件读取）
std::string g_FrontAddress;
std::string g_BrokerID;
std::string g_UserID;
std::string g_Password;
std::string g_AppID;
std::string g_AuthCode;
std::string g_UserProductInfo;

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
        std::cout << "\n========================================" << std::endl;
        std::cout << "=== 融航交易API连接成功 ===" << std::endl;
        std::cout << "========================================\n" << std::endl;
        g_bTraderConnected = true;
        g_bTraderAuthenticated = false;
        g_bTraderLoggedIn = false;
        
        // 先发送认证请求
        reqAuthenticate();
    }

    // 发送认证请求
    void reqAuthenticate()
    {
        std::cout << "[步骤1] 发送认证请求..." << std::endl;
        std::cout << "  BrokerID: " << g_BrokerID << std::endl;
        std::cout << "  UserID: " << g_UserID << std::endl;
        std::cout << "  AppID: " << g_AppID << std::endl;
        std::cout << "  AuthCode: " << g_AuthCode << std::endl;
        
        CThostFtdcReqAuthenticateField req;
        memset(&req, 0, sizeof(req));
        strncpy(req.BrokerID, g_BrokerID.c_str(), sizeof(req.BrokerID) - 1);
        strncpy(req.UserID, g_UserID.c_str(), sizeof(req.UserID) - 1);
        strncpy(req.AppID, g_AppID.c_str(), sizeof(req.AppID) - 1);
        strncpy(req.AuthCode, g_AuthCode.c_str(), sizeof(req.AuthCode) - 1);
        if (!g_UserProductInfo.empty()) {
            strncpy(req.UserProductInfo, g_UserProductInfo.c_str(), sizeof(req.UserProductInfo) - 1);
        }
        
        int ret = m_pTraderApi->ReqAuthenticate(&req, ++g_nRequestID);
        if (ret != 0) {
            std::cout << "  ❌ 认证请求发送失败，错误代码: " << ret << std::endl;
        } else {
            std::cout << "  ✓ 认证请求已发送，等待响应..." << std::endl;
        }
    }

    // 认证响应
    virtual void OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField,
                                   CThostFtdcRspInfoField *pRspInfo,
                                   int nRequestID, bool bIsLast) override
    {
        std::cout << "\n[步骤2] 收到认证响应" << std::endl;
        
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            std::cout << "  ❌ 认证失败！" << std::endl;
            std::cout << "  错误代码: " << pRspInfo->ErrorID << std::endl;
            std::cout << "  错误信息: " << pRspInfo->ErrorMsg << std::endl;
            std::cout << "\n========================================" << std::endl;
            std::cout << "认证验证失败，请检查认证信息是否正确" << std::endl;
            std::cout << "========================================\n" << std::endl;
        } else {
            std::cout << "  ✓ 认证成功！" << std::endl;
            if (pRspAuthenticateField) {
                std::cout << "  经纪公司代码: " << pRspAuthenticateField->BrokerID << std::endl;
                std::cout << "  用户代码: " << pRspAuthenticateField->UserID << std::endl;
                std::cout << "  用户端产品信息: " << pRspAuthenticateField->UserProductInfo << std::endl;
                std::cout << "  App代码: " << pRspAuthenticateField->AppID << std::endl;
                std::cout << "  App类型: " << pRspAuthenticateField->AppType << std::endl;
            }
            g_bTraderAuthenticated = true;
            
            // 认证成功后发送登录请求
            std::cout << "\n[步骤3] 认证成功，继续登录流程..." << std::endl;
            reqUserLogin();
        }
    }

    // 发送登录请求
    void reqUserLogin()
    {
        std::cout << "  发送登录请求..." << std::endl;
        std::cout << "  BrokerID: " << g_BrokerID << std::endl;
        std::cout << "  UserID: " << g_UserID << std::endl;
        
        CThostFtdcReqUserLoginField req;
        memset(&req, 0, sizeof(req));
        strncpy(req.BrokerID, g_BrokerID.c_str(), sizeof(req.BrokerID) - 1);
        strncpy(req.UserID, g_UserID.c_str(), sizeof(req.UserID) - 1);
        strncpy(req.Password, g_Password.c_str(), sizeof(req.Password) - 1);
        
        int ret = m_pTraderApi->ReqUserLogin(&req, ++g_nRequestID);
        if (ret != 0) {
            std::cout << "  ❌ 登录请求发送失败，错误代码: " << ret << std::endl;
        } else {
            std::cout << "  ✓ 登录请求已发送，等待响应..." << std::endl;
        }
    }

    // 连接断开回调
    virtual void OnFrontDisconnected(int nReason) override
    {
        std::cout << "\n=== 交易API连接断开 ===" << std::endl;
        std::cout << "断开原因: " << nReason << std::endl;
        g_bTraderConnected = false;
        g_bTraderAuthenticated = false;
        g_bTraderLoggedIn = false;
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
        std::cout << "\n[步骤4] 收到登录响应" << std::endl;
        
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            std::cout << "  ❌ 登录失败！" << std::endl;
            std::cout << "  错误代码: " << pRspInfo->ErrorID << std::endl;
            std::cout << "  错误信息: " << pRspInfo->ErrorMsg << std::endl;
        } else {
            std::cout << "  ✓ 登录成功！" << std::endl;
            std::cout << "\n=== 登录信息 ===" << std::endl;
            std::cout << "交易日: " << pRspUserLogin->TradingDay << std::endl;
            std::cout << "登录成功时间: " << pRspUserLogin->LoginTime << std::endl;
            std::cout << "经纪公司代码: " << pRspUserLogin->BrokerID << std::endl;
            std::cout << "用户代码: " << pRspUserLogin->UserID << std::endl;
            std::cout << "交易系统名称: " << pRspUserLogin->SystemName << std::endl;
            std::cout << "前置编号: " << pRspUserLogin->FrontID << std::endl;
            std::cout << "会话编号: " << pRspUserLogin->SessionID << std::endl;
            std::cout << "最大报单引用: " << pRspUserLogin->MaxOrderRef << std::endl;
            std::cout << "----------------------------------------" << std::endl;
            g_bTraderLoggedIn = true;
            
            std::cout << "\n========================================" << std::endl;
            std::cout << "✓ 融航交易API认证验证成功！" << std::endl;
            std::cout << "  认证流程: 连接 -> 认证 -> 登录" << std::endl;
            std::cout << "  所有步骤均已完成" << std::endl;
            std::cout << "========================================\n" << std::endl;
        }
    }

    // 错误响应
    virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, 
                           int nRequestID, bool bIsLast) override
    {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            std::cout << "\n=== 收到错误响应 ===" << std::endl;
            std::cout << "请求ID: " << nRequestID << std::endl;
            std::cout << "错误代码: " << pRspInfo->ErrorID << std::endl;
            std::cout << "错误信息: " << pRspInfo->ErrorMsg << std::endl;
        }
    }
};

// 信号处理函数
void signalHandler(int signum)
{
    std::cout << "\n=== 收到退出信号（" << signum << "），正在关闭... ===" << std::endl;
    
    // 设置退出标志
    g_bShouldExit = true;
    
    // 释放API资源
    if (g_pTraderApi) {
        std::cout << "正在释放API资源..." << std::endl;
        g_pTraderApi->RegisterSpi(nullptr);  // 先取消注册回调，避免回调中访问已释放的资源
        g_pTraderApi->Release();
        g_pTraderApi = nullptr;
        std::cout << "API资源已释放" << std::endl;
    }
    
    // 使用_exit强制退出，避免清理操作阻塞
    std::cout << "程序退出" << std::endl;
    std::cout.flush();  // 确保输出被刷新
    exit(0);
}

int main(int argc, char* argv[])
{
    // 注册信号处理函数
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 确定配置文件路径
    std::string configFile = "config.json";
    if (argc > 1) {
        configFile = argv[1];
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "  融航交易API认证验证Demo" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n读取配置文件: " << configFile << std::endl;
    
    // 读取配置文件
    if (!parseJsonConfig(configFile, g_FrontAddress, g_BrokerID, g_UserID, 
                        g_Password, g_AppID, g_AuthCode, g_UserProductInfo)) {
        std::cerr << "\n使用示例: " << argv[0] << " [config.json]" << std::endl;
        std::cerr << "默认配置文件: config.json" << std::endl;
        return -1;
    }
    
    std::cout << "✓ 配置文件读取成功" << std::endl;
    std::cout << "\n认证信息:" << std::endl;
    std::cout << "  前置地址: " << g_FrontAddress << std::endl;
    std::cout << "  BrokerID: " << g_BrokerID << std::endl;
    std::cout << "  UserID: " << g_UserID << std::endl;
    std::cout << "  AppID: " << g_AppID << std::endl;
    std::cout << "  AuthCode: " << g_AuthCode << std::endl;
    std::cout << "\n正在创建交易API实例..." << std::endl;

    // 创建交易API实例
    g_pTraderApi = CThostFtdcTraderApi::CreateFtdcTraderApi("./");
    if (!g_pTraderApi) {
        std::cout << "❌ 创建交易API实例失败" << std::endl;
        return -1;
    }
    std::cout << "✓ 交易API实例创建成功" << std::endl;

    // 创建回调实例
    CTraderSpi traderSpi(g_pTraderApi);
    g_pTraderApi->RegisterSpi(&traderSpi);

    // 注册前置地址
    std::cout << "\n注册前置地址: " << g_FrontAddress << std::endl;
    g_pTraderApi->RegisterFront((char*)g_FrontAddress.c_str());

    // 初始化API
    std::cout << "初始化API..." << std::endl;
    g_pTraderApi->Init();

    std::cout << "\n正在连接服务器，等待认证流程..." << std::endl;
    std::cout << "（按 Ctrl+C 退出）\n" << std::endl;

    // 主循环
    while (!g_bShouldExit) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (g_bShouldExit) {
            break;
        }
        
        if (!g_bTraderConnected) {
            continue;
        }
        
        if (!g_bTraderAuthenticated) {
            continue;
        }
        
        if (!g_bTraderLoggedIn) {
            continue;
        }
        
        // 所有步骤完成，保持运行
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 清理资源
    if (g_pTraderApi) {
        std::cout << "\n正在清理资源..." << std::endl;
        g_pTraderApi->RegisterSpi(nullptr);
        g_pTraderApi->Release();
        g_pTraderApi = nullptr;
    }
    
    std::cout << "程序正常退出" << std::endl;
    return 0;
}
