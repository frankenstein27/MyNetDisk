#ifndef ABSTRACTMONITORPROVIDER_H
#define ABSTRACTMONITORPROVIDER_H

#include <QtGlobal>

// 统一的系统资源快照结构体
struct SystemSnapshot {
    double cpuUsage;       // CPU 利用率 (0.0 ~ 100.0)
    double memoryTotalGB;  // 总物理内存 (GB)
    double memoryUsedGB;   // 已用物理内存 (GB)
    double diskTotalGB;    // 总磁盘 (GB)
    double diskUsedGB;     // 已用磁盘 (GB)
    qint64 networkIn;      // 网络接收字节 (自系统启动以来的累计值)
    qint64 networkOut;     // 网络发送字节 (自系统启动以来的累计值)
};

// 抽象采集基类（完全不包含任何平台特有头文件）
class AbstractMonitorProvider {
   public:
    virtual ~AbstractMonitorProvider() = default;

    // 纯虚函数：由不同平台子类负责具体实现
    virtual SystemSnapshot collect() = 0;
};

#endif  // ABSTRACTMONITORPROVIDER_H
