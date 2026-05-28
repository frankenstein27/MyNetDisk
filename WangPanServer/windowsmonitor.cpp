#include "windowsmonitor.h"

// 平台相关实现均在对应 #ifdef 块中，头文件完全隔离 Windows 头文件
#ifdef Q_OS_WIN
#include <windows.h>

#include <QStorageInfo>

// 将 Windows FILETIME 转换为 64 位无符号整数（模块内部辅助函数）
static ULONGLONG fileTimeToUInt64(const FILETIME& ft) { return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime; }

// 静态成员定义
unsigned long long WindowsMonitorTool::m_prevIdleTime = 0;
unsigned long long WindowsMonitorTool::m_prevKernelTime = 0;
unsigned long long WindowsMonitorTool::m_prevUserTime = 0;
bool WindowsMonitorTool::m_prevCpuValid = false;

SystemSnapshot WindowsMonitorTool::collect() {
    SystemSnapshot snap;
    snap.cpuUsage = 0.0;
    snap.memoryTotalMB = 0.0;
    snap.memoryUsedMB = 0.0;
    snap.diskTotalGB = 0.0;
    snap.diskUsagePercent = 0.0;
    snap.networkIn = 0;
    snap.networkOut = 0;

    // --- CPU 利用率 ---
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        ULONGLONG idle = fileTimeToUInt64(idleTime);
        ULONGLONG kernel = fileTimeToUInt64(kernelTime);
        ULONGLONG user = fileTimeToUInt64(userTime);

        if (m_prevCpuValid) {
            ULONGLONG deltaIdle = idle - m_prevIdleTime;
            ULONGLONG deltaKernel = kernel - m_prevKernelTime;
            ULONGLONG deltaUser = user - m_prevUserTime;
            ULONGLONG deltaTotal = deltaKernel + deltaUser;
            if (deltaTotal > 0) {
                snap.cpuUsage = static_cast<double>(deltaTotal - deltaIdle) / deltaTotal * 100.0;
            }
        }
        m_prevIdleTime = idle;
        m_prevKernelTime = kernel;
        m_prevUserTime = user;
        m_prevCpuValid = true;
    }

    // --- 内存 (MB) ---
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        snap.memoryTotalMB = static_cast<double>(memStatus.ullTotalPhys) / (1024.0 * 1024.0);
        snap.memoryUsedMB = static_cast<double>(memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024.0 * 1024.0);
    }

    // --- 磁盘使用率 (%) ---
    QStorageInfo storage = QStorageInfo::root();
    if (storage.isValid()) {
        double totalBytes = static_cast<double>(storage.bytesTotal());
        double usedBytes = static_cast<double>(storage.bytesTotal() - storage.bytesAvailable());
        snap.diskTotalGB = totalBytes / (1024.0 * 1024.0 * 1024.0);
        snap.diskUsagePercent = (totalBytes > 0) ? (usedBytes / totalBytes) * 100.0 : 0.0;
    }

    return snap;
}

#else  // Linux/其他平台回退实现

SystemSnapshot WindowsMonitorTool::collect() {
    SystemSnapshot snap;
    snap.cpuUsage = 0.0;
    snap.memoryTotalMB = 0.0;
    snap.memoryUsedMB = 0.0;
    snap.diskTotalGB = 0.0;
    snap.diskUsagePercent = 0.0;
    snap.networkIn = 0;
    snap.networkOut = 0;
    return snap;
}

#endif
