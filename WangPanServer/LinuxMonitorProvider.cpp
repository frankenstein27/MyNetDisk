#include "LinuxMonitorProvider.h"

#include <QFile>
#include <QStorageInfo>
#include <QTextStream>
#include <QtDebug>

SystemSnapshot LinuxMonitorProvider::collect() {
    SystemSnapshot snap;
    snap.cpuUsage = 0.0;
    snap.memoryTotalMB = 0.0;
    snap.memoryUsedMB = 0.0;
    snap.diskTotalGB = 0.0;
    snap.diskUsagePercent = 0.0;
    snap.networkIn = 0;
    snap.networkOut = 0;

    // ------------------------------------------------------------------
    // 1. 解析 /proc/stat 获取 CPU 利用率（两次采样间的增量）
    //    hqtop 同款算法：累加所有字段得 curTotalCpu，idle 位于第 5 列（索引 4）
    // ------------------------------------------------------------------
    QFile statFile("/proc/stat");
    if (statFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&statFile);
        // Qt::SkipEmptyParts 兼容字段间可能存在的多余空格
        QStringList cpuList = stream.readLine().split(' ', Qt::SkipEmptyParts);
        if (cpuList.size() >= 5 && cpuList[0] == "cpu") {
            double curIdleCpu = cpuList[4].toDouble();  // idle
            double curTotalCpu = 0.0;
            for (int i = 1; i < cpuList.size(); ++i) {
                curTotalCpu += cpuList[i].toDouble();
            }

            double deltaTotal = curTotalCpu - m_prevTotalCpu;
            double deltaIdle = curIdleCpu - m_prevIdleCpu;
            if (deltaTotal > 0.0) {
                snap.cpuUsage = (1.0 - (deltaIdle / deltaTotal)) * 100.0;
            }
            m_prevTotalCpu = curTotalCpu;
            m_prevIdleCpu = curIdleCpu;
        }
        statFile.close();
    }

    // ------------------------------------------------------------------
    // 2. 解析 /proc/meminfo 获取内存 (MemTotal / MemAvailable)
    //    hqtop 同款算法：MemTotal - MemAvailable = 已用内存
    // ------------------------------------------------------------------
    QFile memFile("/proc/meminfo");
    if (memFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&memFile);
        double totalKB = 0.0, availKB = 0.0;
        QString line;
        while (stream.readLineInto(&line)) {
            if (line.startsWith("MemTotal:")) {
                totalKB = line.split(' ', Qt::SkipEmptyParts).value(1).toDouble();
            } else if (line.startsWith("MemAvailable:")) {
                availKB = line.split(' ', Qt::SkipEmptyParts).value(1).toDouble();
                break;  // 拿到这两个核心字段即可退出
            }
        }
        // KB -> MB: 除以 1024
        snap.memoryTotalMB = totalKB / 1024.0;
        snap.memoryUsedMB = (totalKB - availKB) / 1024.0;
        memFile.close();
    }

    // ------------------------------------------------------------------
    // 3. 跨平台磁盘使用率 (QStorageInfo 在 Linux 下同样精准)
    // ------------------------------------------------------------------
    QStorageInfo storage = QStorageInfo::root();
    if (storage.isValid() && storage.isReady()) {
        double totalBytes = static_cast<double>(storage.bytesTotal());
        double usedBytes = static_cast<double>(storage.bytesTotal() - storage.bytesAvailable());
        snap.diskTotalGB = totalBytes / (1024.0 * 1024.0 * 1024.0);
        snap.diskUsagePercent = (totalBytes > 0) ? (usedBytes / totalBytes) * 100.0 : 0.0;
    }

    // ------------------------------------------------------------------
    // 4. 网络流量采集 (/proc/net/dev)
    //    累计值由 Monitor 层做两次采集的差值计算
    // ------------------------------------------------------------------
    QFile netFile("/proc/net/dev");
    if (netFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&netFile);
        QString line;
        qint64 totalIn = 0, totalOut = 0;
        while (stream.readLineInto(&line)) {
            // 跳过头部注释行
            if (line.contains(':') && !line.trimmed().startsWith("Inter")) {
                // 格式: 名称: 接收字节 接收包 ... 发送字节 ...
                QStringList cols = line.split(' ', Qt::SkipEmptyParts);
                if (cols.size() >= 10) {
                    // 第 2 列（索引 1）为接收字节，第 10 列（索引 9）为发送字节
                    totalIn += cols.value(1).toLongLong();
                    totalOut += cols.value(9).toLongLong();
                }
            }
        }
        snap.networkIn = totalIn;
        snap.networkOut = totalOut;
        netFile.close();
    }

    return snap;
}
