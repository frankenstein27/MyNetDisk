#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QObject>
#include <QString>

class LogManager : public QObject
{
    Q_OBJECT

public:
    static LogManager *instance();
    ~LogManager();

    void logUserAction(const QString &username, const QString &action, const QString &targetType, const QString &targetId, const QString &details, const QString &ip);
    void logSystemEvent(const QString &event, const QString &details);

private:
    LogManager(QObject *parent = nullptr);
    static LogManager *m_instance;

    QString m_logFile;
};

#endif // LOGMANAGER_H