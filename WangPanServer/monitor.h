#ifndef MONITOR_H
#define MONITOR_H

#include <QObject>
#include <QTimer>

class Monitor : public QObject
{
    Q_OBJECT

public:
    static Monitor *instance();
    ~Monitor();

    void start();
    void stop();

    float getCpuUsage();
    float getMemoryUsage();
    float getDiskUsage();
    qint64 getNetworkIn();
    qint64 getNetworkOut();
    int getConnectionCount();

signals:
    void monitoringUpdate(float cpuUsage, float memoryUsage, float diskUsage, qint64 networkIn, qint64 networkOut, int connectionCount);

private slots:
    void onTimerTimeout();

private:
    Monitor(QObject *parent = nullptr);
    static Monitor *m_instance;

    QTimer *m_timer;
    float m_cpuUsage;
    float m_memoryUsage;
    float m_diskUsage;
    qint64 m_networkIn;
    qint64 m_networkOut;
    int m_connectionCount;
};

#endif // MONITOR_H