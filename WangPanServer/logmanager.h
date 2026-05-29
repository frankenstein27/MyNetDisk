#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QObject>
#include <QString>

// 单例日志类，负责记录用户操作日志和系统事件日志，提供接口供其他模块调用，日志可以保存到文件server.log或数据库中
class LogManager : public QObject {
    Q_OBJECT

   public:
    static LogManager* instance();
    ~LogManager();

    void logUserAction(const QString& username, const QString& action, const QString& targetType, const QString& targetId, const QString& details, const QString& ip);
    void logSystemEvent(const QString& event, const QString& details);

   private:
    LogManager(QObject* parent = nullptr);
    static LogManager* m_instance;

    QString m_logFile;
};

#endif  // LOGMANAGER_H