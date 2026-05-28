#include "WindowsMonitorProvider.h"

#include "windowsmonitor.h"

// Windows 平台的 collect() 直接委托给已有的 WindowsMonitorTool
// WindowsMonitorTool 内部通过 #ifdef Q_OS_WIN 封装了 Win32 API 调用
SystemSnapshot WindowsMonitorProvider::collect() { return WindowsMonitorTool::collect(); }

// 静态成员定义（与 WindowsMonitorTool 的对应成员完全解耦）
unsigned long long WindowsMonitorProvider::m_prevIdleTime = 0;
unsigned long long WindowsMonitorProvider::m_prevKernelTime = 0;
unsigned long long WindowsMonitorProvider::m_prevUserTime = 0;
bool WindowsMonitorProvider::m_prevCpuValid = false;
