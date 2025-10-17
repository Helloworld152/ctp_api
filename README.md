# CTP API 行情获取程序

## 项目结构

```
/home/rying/ctp_api/
├── 📁 include/                       # 头文件目录
│   ├── ThostFtdcUserApiDataType.h    # 数据类型定义
│   ├── ThostFtdcUserApiStruct.h      # 结构体定义  
│   ├── ThostFtdcMdApi.h              # 行情API接口
│   └── ThostFtdcTraderApi.h          # 交易API接口
├── 📁 lib/                          # 动态库目录
│   ├── thostmduserapi_se.so          # 行情API动态库
│   └── thosttraderapi_se.so          # 交易API动态库
├── 📁 src/                          # 源代码目录
│   ├── md_client.cpp                 # 完整版行情客户端
│   └── simple_md_client.cpp          # 简化版行情客户端
├── 📁 config/                       # 配置文件目录
│   ├── config.ini                    # 配置文件
│   ├── error.dtd                     # 错误定义
│   └── error.xml                     # 错误信息
├── 📁 build/                        # 构建输出目录 (自动生成)
│   └── bin/                         # 可执行文件目录
├── 🔧 构建系统
│   ├── CMakeLists.txt               # CMake配置文件
│   ├── Makefile                     # Make配置文件
│   ├── build.sh                     # CMake构建脚本
│   ├── clean.sh                     # 清理脚本
│   └── compile.sh                   # 传统编译脚本
└── 📖 README.md                     # 说明文档
```

## 编译和运行

### 方法一：使用CMake (推荐)

```bash
# 使用CMake构建
./build.sh

# 运行程序
cd build/bin
./simple_md_client
# 或
./md_client
```

### 方法二：使用Make

```bash
# 使用Make编译
make

# 运行程序
cd build/bin
./simple_md_client
# 或
./md_client
```

### 方法三：使用传统脚本

```bash
# 使用传统编译脚本
./compile.sh
```

### 清理构建文件

```bash
# 清理CMake构建文件
./clean.sh

# 或清理Make构建文件
make clean
```

## 配置说明

### 服务器地址配置

- **模拟环境**: `tcp://180.168.146.187:10031`
- **实盘环境**: 需要向期货公司申请

### 登录信息

- **BrokerID**: 经纪商代码
- **UserID**: 用户代码  
- **Password**: 密码

### 订阅合约

程序默认订阅以下期货合约：
- `rb2501` - 螺纹钢主力合约
- `hc2501` - 热卷主力合约
- `i2501` - 铁矿石主力合约
- `j2501` - 焦炭主力合约
- `jm2501` - 焦煤主力合约

## 程序功能

### 简化版 (simple_md_client.cpp)
- 基本的连接和登录
- 订阅指定合约行情
- 显示实时行情数据
- 简洁的输出格式

### 完整版 (md_client.cpp)
- 完整的错误处理
- 详细的日志输出
- 心跳监控
- 信号处理
- 更丰富的行情信息显示

## 注意事项

1. **网络连接**: 确保网络连接正常，能够访问CTP服务器
2. **动态库**: 确保 `thostmduserapi_se.so` 文件在当前目录
3. **权限**: 确保程序有读取配置文件和写入日志的权限
4. **合约代码**: 确保订阅的合约代码正确且存在
5. **服务器状态**: 确保CTP服务器正常运行

## 常见问题

### 编译错误
- 检查是否安装了 g++ 编译器
- 检查头文件路径是否正确
- 检查动态库文件是否存在

### 连接失败
- 检查网络连接
- 检查服务器地址是否正确
- 检查防火墙设置

### 登录失败
- 检查用户名密码是否正确
- 检查经纪商代码是否正确
- 检查账户是否有效

### 无行情数据
- 检查合约代码是否正确
- 检查交易时间（非交易时间无行情）
- 检查订阅是否成功

## 扩展功能

可以根据需要添加以下功能：
- 配置文件解析
- 数据库存储
- 图形界面
- 多合约管理
- 历史数据下载
- 技术指标计算
