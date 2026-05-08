#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

class DatabaseManager : public QObject {
    Q_OBJECT

   public:
    static DatabaseManager* instance();
    ~DatabaseManager();

    bool connect();
    void disconnect();
    bool isConnected() const;

    // 用户相关操作
    bool createUser(const QString& username, const QString& email, const QString& passwordHash, const QString& salt, const QString& nickname);
    bool getUser(const QString& username, QString& passwordHash, QString& salt);
    bool getUserInfo(const QString& username, QString& email, QString& nickname, qint64& quota, qint64& usedSpace);
    bool updateLastLogin(const QString& username);
    bool updatePassword(const QString& username, const QString& passwordHash, const QString& salt);
    bool updateUserInfo(const QString& username, const QString& email, const QString& nickname);
    bool deleteUser(const QString& username);

    // 文件相关操作
    bool addFile(const QString& username, const QString& filename, const QString& path, qint64 size, const QString& type, int directoryId);
    bool getFileList(const QString& username, const QString& directory, QList<QMap<QString, QVariant>>& fileList);
    bool createDirectory(const QString& username, const QString& path);
    bool getDirectoryList(const QString& username, int parentId, QList<QMap<QString, QVariant>>& directoryList);
    int getRootDirectoryId(const QString& username);

    // 日志操作记录
    void logAction(const QString& username, const QString& action, const QString& targetType, const QString& targetId, const QString& details, const QString& ip);

    void createTables();

   private:
    DatabaseManager(QObject* parent = nullptr);
    static DatabaseManager* m_instance;

    QSqlDatabase m_db;
};

#endif  // DATABASEMANAGER_H
