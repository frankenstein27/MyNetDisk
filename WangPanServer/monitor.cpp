#include "monitor.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "databasemanager.h"

Monitor* Monitor::m_instance = nullptr;

Monitor::Monitor(QObject* parent) : QObject(parent), m_cpuUsage(0), m_memoryUsage(0), m_diskUsage(0), m_networkIn(0), m_networkOut(0), m_connectionCount(0) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Monitor::onTimerTimeout);
}

Monitor::~Monitor() { delete m_timer; }

Monitor* Monitor::instance() {
    if (!m_instance) {
        m_instance = new Monitor();
    }
    return m_instance;
}

void Monitor::start() {
    m_timer->start(5000);  // 每5秒监控一次
}

void Monitor::stop() { m_timer->stop(); }

float Monitor::getCpuUsage() { return m_cpuUsage; }

float Monitor::getMemoryUsage() { return m_memoryUsage; }

float Monitor::getDiskUsage() { return m_diskUsage; }

qint64 Monitor::getNetworkIn() { return m_networkIn; }

qint64 Monitor::getNetworkOut() { return m_networkOut; }

int Monitor::getConnectionCount() { return m_connectionCount; }

void Monitor::setConnectionCount(int count) { m_connectionCount = count; }

void Monitor::onTimerTimeout() {
    // 使用 WindowsMonitorTool 采集真实系统数据
    SystemSnapshot snap = WindowsMonitorTool::collect();

    m_cpuUsage = static_cast<float>(snap.cpuUsage);
    m_memoryUsage = static_cast<float>(snap.memoryUsedGB);
    m_diskUsage = static_cast<float>(snap.diskUsedGB);
    m_networkIn = snap.networkIn;
    m_networkOut = snap.networkOut;

    emit monitoringUpdate(m_cpuUsage, m_memoryUsage, m_diskUsage, m_networkIn, m_networkOut, m_connectionCount);

    // 定期写入 server_monitor 表
    QSqlQuery query;
    query.prepare(
        "INSERT INTO server_monitor (cpu_usage, memory_usage, disk_usage, network_in, network_out, connection_count) "
        "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(m_cpuUsage);
    query.addBindValue(m_memoryUsage);
    query.addBindValue(m_diskUsage);
    query.addBindValue(m_networkIn);
    query.addBindValue(m_networkOut);
    query.addBindValue(m_connectionCount);
    query.exec();
}
