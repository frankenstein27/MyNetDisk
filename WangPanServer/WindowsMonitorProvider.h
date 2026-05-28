#ifndef WINDOWSMONITORPROVIDER_H
#define WINDOWSMONITORPROVIDER_H

#include "AbstractMonitorProvider.h"

// Windows 平台监控提供者（所有 Win32 API 调用封装在 .cpp 中）
class WindowsMonitorProvider : public AbstractMonitorProvider {
   public:
    WindowsMonitorProvider() = default;
    ~WindowsMonitorProvider() = default;

    SystemSnapshot collect() override;

   private:
    // 用于两轮采样间计算 CPU 增量的历史值（平台无关类型）
    static unsigned long long m_prevIdleTime;
    static unsigned long long m_prevKernelTime;
    static unsigned long long m_prevUserTime;
    static bool m_prevCpuValid;
};

#endif  // WINDOWSMONITORPROVIDER_H
