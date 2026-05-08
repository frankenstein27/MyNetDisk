#ifndef WINDOWSMONITOR_H
#define WINDOWSMONITOR_H

#include <QtGlobal>

// 系统资源快照结构体
struct SystemSnapshot {
    double cpuUsage;       // CPU 利用率百分比
    double memoryTotalGB;  // 总物理内存 (GB)
    double memoryUsedGB;   // 已用物理内存 (GB)
    double diskTotalGB;    // 总磁盘 (GB)
    double diskUsedGB;     // 已用磁盘 (GB)
    qint64 networkIn;      // 网络接收字节
    qint64 networkOut;     // 网络发送字节
};

// 跨平台系统监控工具类（所有平台相关代码在 .cpp 中）
class WindowsMonitorTool {
   public:
    static SystemSnapshot collect();

   private:
    // 用于存储两轮采样间的 Windows 时间值（平台无关类型）
    static unsigned long long m_prevIdleTime;
    static unsigned long long m_prevKernelTime;
    static unsigned long long m_prevUserTime;
    static bool m_prevCpuValid;
};

#endif  // WINDOWSMONITOR_H
