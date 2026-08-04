# opstools

用于启动和停止程序：

- `start.sh`：后台启动程序，生成 `pid` 文件和日志
- `stop.sh`：按 `pid` 停止程序

## 使用

启动：

```bash
bash start.sh
```

停止：

```bash
bash stop.sh
```

## 换程序时改哪里

主要改 `start.sh` 和 `stop.sh` 顶部配置：

1. `APP_NAME`
2. `ARGS`

说明：

- `APP_NAME`：程序名，两个脚本里都要保持一致
- `ARGS`：启动参数
- `APP` 默认是 `"$DIR/$APP_NAME"`，通常不用改

## crontab

编辑定时任务：

```bash
crontab -e
```

开机自动启动：

```bash
@reboot bash /home/ruanying/ctp_api/opstools/start.sh
```

查看当前定时任务：

```bash
crontab -l
```
