#include "monitor.h"

Monitor *Monitor::m_instance = nullptr;

Monitor::Monitor(QObject *parent) : QObject(parent),
    m_cpuUsage(0), m_memoryUsage(0), m_diskUsage(0), m_networkIn(0), m_networkOut(0), m_connectionCount(0)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Monitor::onTimerTimeout);
}

Monitor::~Monitor()
{
    delete m_timer;
}

Monitor *Monitor::instance()
{
    if (!m_instance) {
        m_instance = new Monitor();
    }
    return m_instance;
}

void Monitor::start()
{
    m_timer->start(5000); // 每5秒监控一次
}

void Monitor::stop()
{
    m_timer->stop();
}

float Monitor::getCpuUsage()
{
    return m_cpuUsage;
}

float Monitor::getMemoryUsage()
{
    return m_memoryUsage;
}

float Monitor::getDiskUsage()
{
    return m_diskUsage;
}

qint64 Monitor::getNetworkIn()
{
    return m_networkIn;
}

qint64 Monitor::getNetworkOut()
{
    return m_networkOut;
}

int Monitor::getConnectionCount()
{
    return m_connectionCount;
}

void Monitor::onTimerTimeout()
{
    // 实现监控逻辑
    // 这里只是示例，实际需要根据平台实现具体的监控
    m_cpuUsage = 0.0;
    m_memoryUsage = 0.0;
    m_diskUsage = 0.0;
    m_networkIn = 0;
    m_networkOut = 0;
    m_connectionCount = 0;

    emit monitoringUpdate(m_cpuUsage, m_memoryUsage, m_diskUsage, m_networkIn, m_networkOut, m_connectionCount);
}
