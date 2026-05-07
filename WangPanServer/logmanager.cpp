#include "logmanager.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>

LogManager *LogManager::m_instance = nullptr;

LogManager::LogManager(QObject *parent) : QObject(parent)
{
    m_logFile = "server.log";
}

LogManager::~LogManager()
{
}

LogManager *LogManager::instance()
{
    if (!m_instance) {
        m_instance = new LogManager();
    }
    return m_instance;
}

void LogManager::logUserAction(const QString &username, const QString &action, const QString &targetType, const QString &targetId, const QString &details, const QString &ip)
{
    QFile file(m_logFile);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << " | USER | " << username << " | " << action << " | " << targetType << " | " << targetId << " | " << details << " | " << ip << "\n";
        file.close();
    }
}

void LogManager::logSystemEvent(const QString &event, const QString &details)
{
    QFile file(m_logFile);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << " | SYSTEM | " << event << " | " << details << "\n";
        file.close();
    }
}
