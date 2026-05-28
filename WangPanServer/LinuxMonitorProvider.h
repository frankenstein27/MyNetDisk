#ifndef LINUXMONITORPROVIDER_H
#define LINUXMONITORPROVIDER_H

#include "AbstractMonitorProvider.h"

class LinuxMonitorProvider : public AbstractMonitorProvider {
   public:
    LinuxMonitorProvider() = default;
    ~LinuxMonitorProvider() = default;

    SystemSnapshot collect() override;

   private:
    // 用于两轮采样间计算 CPU 增量的历史值
    double m_prevTotalCpu = 0.0;
    double m_prevIdleCpu = 0.0;
};

#endif  // LINUXMONITORPROVIDER_H
