#ifndef WINDOWSMONITOR_H
#define WINDOWSMONITOR_H

#include "AbstractMonitorProvider.h"

// 系统监控工具类（所有平台相关代码在 .cpp 中通过 #ifdef Q_OS_WIN 隔离）
// 保留此类用于向后兼容；新的采集逻辑请使用 AbstractMonitorProvider 子类
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
