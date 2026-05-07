#include "databasemanager.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

DatabaseManager *DatabaseManager::m_instance = nullptr;

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    disconnect();
}

DatabaseManager *DatabaseManager::instance()
{
    if (!m_instance) {
        m_instance = new DatabaseManager();
    }
    return m_instance;
}

bool DatabaseManager::connect()
{
    m_db = QSqlDatabase::addDatabase("QMYSQL");
    m_db.setHostName("localhost");
    m_db.setDatabaseName("wangpan");
    m_db.setUserName("root");
    m_db.setPassword("hebo");

    if (!m_db.open()) {
        qWarning("Failed to connect to database: %s", qPrintable(m_db.lastError().text()));
        return false;
    }

    // 创建表结构
    createTables();
    return true;
}

void DatabaseManager::disconnect()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::isConnected() const
{
    return m_db.isOpen();
}

void DatabaseManager::createTables()
{
    QSqlQuery query;

    // 创建用户表
    query.exec("CREATE TABLE IF NOT EXISTS users (" 
               "id INT AUTO_INCREMENT PRIMARY KEY, "
               "username VARCHAR(50) UNIQUE NOT NULL, "
               "email VARCHAR(100) UNIQUE NOT NULL, "
               "password_hash VARCHAR(255) NOT NULL, "
               "salt VARCHAR(50) NOT NULL, "
               "nickname VARCHAR(50) NOT NULL, "
               "avatar VARCHAR(255), "
               "quota BIGINT NOT NULL DEFAULT 10737418240, "
               "used_space BIGINT NOT NULL DEFAULT 0, "
               "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
               "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
               "last_login DATETIME, "
               "status TINYINT NOT NULL DEFAULT 1 "
               ")");


    // 创建目录表
    query.exec("CREATE TABLE IF NOT EXISTS directories (" 
               "id INT AUTO_INCREMENT PRIMARY KEY, "
               "name VARCHAR(255) NOT NULL, "
               "parent_id INT, "
               "user_id INT NOT NULL, "
               "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
               "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
               "FOREIGN KEY (parent_id) REFERENCES directories(id), "
               "FOREIGN KEY (user_id) REFERENCES users(id) "
               ")");


    // 创建文件表
    query.exec("CREATE TABLE IF NOT EXISTS files (" 
               "id INT AUTO_INCREMENT PRIMARY KEY, "
               "name VARCHAR(255) NOT NULL, "
               "path VARCHAR(512) NOT NULL, "
               "size BIGINT NOT NULL, "
               "type VARCHAR(50) NOT NULL, "
               "directory_id INT NOT NULL, "
               "user_id INT NOT NULL, "
               "hash VARCHAR(255), "
               "upload_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
               "last_modified DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
               "status TINYINT NOT NULL DEFAULT 1, "
               "FOREIGN KEY (directory_id) REFERENCES directories(id), "
               "FOREIGN KEY (user_id) REFERENCES users(id) "
               ")");


    // 创建权限表
    query.exec("CREATE TABLE IF NOT EXISTS permissions (" 
               "id INT AUTO_INCREMENT PRIMARY KEY, "
               "entity_type TINYINT NOT NULL, "
               "entity_id INT NOT NULL, "
               "user_id INT NOT NULL, "
               "permission TINYINT NOT NULL, "
               "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
               "FOREIGN KEY (user_id) REFERENCES users(id) "
               ")");


    // 创建共享表
    query.exec("CREATE TABLE IF NOT EXISTS shares (" 
               "id INT AUTO_INCREMENT PRIMARY KEY, "
               "entity_type TINYINT NOT NULL, "
               "entity_id INT NOT NULL, "
               "user_id INT NOT NULL, "
               "share_code VARCHAR(50) UNIQUE NOT NULL, "
               "expire_time DATETIME, "
               "password VARCHAR(255), "
               "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
               "status TINYINT NOT NULL DEFAULT 1, "
               "FOREIGN KEY (user_id) REFERENCES users(id) "
               ")");


    // 创建日志表
    query.exec("CREATE TABLE IF NOT EXISTS logs (" 
               "id INT AUTO_INCREMENT PRIMARY KEY, "
               "user_id INT, "
               "action VARCHAR(50) NOT NULL, "
               "target_type TINYINT, "
               "target_id INT, "
               "details TEXT, "
               "ip VARCHAR(50), "
               "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
               "FOREIGN KEY (user_id) REFERENCES users(id) "
               ")");


    // 创建服务器监控表
    query.exec("CREATE TABLE IF NOT EXISTS server_monitor (" 
               "id INT AUTO_INCREMENT PRIMARY KEY, "
               "cpu_usage FLOAT NOT NULL, "
               "memory_usage FLOAT NOT NULL, "
               "disk_usage FLOAT NOT NULL, "
               "network_in BIGINT NOT NULL, "
               "network_out BIGINT NOT NULL, "
               "connection_count INT NOT NULL, "
               "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP "
               ")");

}

bool DatabaseManager::createUser(const QString &username, const QString &email, const QString &passwordHash, const QString &salt, const QString &nickname)
{
    QSqlQuery query;
    query.prepare("INSERT INTO users (username, email, password_hash, salt, nickname) VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(username);
    query.addBindValue(email);
    query.addBindValue(passwordHash);
    query.addBindValue(salt);
    query.addBindValue(nickname);

    if (!query.exec()) {
        qWarning("Failed to create user: %s", qPrintable(query.lastError().text()));
        return false;
    }

    // 创建用户根目录
    int userId = query.lastInsertId().toInt();
    query.prepare("INSERT INTO directories (name, parent_id, user_id) VALUES (?, ?, ?)");
    query.addBindValue("root");
    query.addBindValue(QVariant(QVariant::Int)); // NULL parent_id
    query.addBindValue(userId);

    if (!query.exec()) {
        qWarning("Failed to create root directory: %s", qPrintable(query.lastError().text()));
        return false;
    }

    return true;
}

bool DatabaseManager::getUser(const QString &username, QString &passwordHash, QString &salt)
{
    QSqlQuery query;
    query.prepare("SELECT password_hash, salt FROM users WHERE username = ? AND status = 1");
    query.addBindValue(username);

    if (!query.exec()) {
        qWarning("Failed to get user: %s", qPrintable(query.lastError().text()));
        return false;
    }

    if (query.next()) {
        passwordHash = query.value(0).toString();
        salt = query.value(1).toString();
        return true;
    }

    return false;
}

bool DatabaseManager::getUserInfo(const QString &username, QString &email, QString &nickname, qint64 &quota, qint64 &usedSpace)
{
    QSqlQuery query;
    query.prepare("SELECT email, nickname, quota, used_space FROM users WHERE username = ? AND status = 1");
    query.addBindValue(username);

    if (!query.exec()) {
        qWarning("Failed to get user info: %s", qPrintable(query.lastError().text()));
        return false;
    }

    if (query.next()) {
        email = query.value(0).toString();
        nickname = query.value(1).toString();
        quota = query.value(2).toLongLong();
        usedSpace = query.value(3).toLongLong();
        return true;
    }

    return false;
}

bool DatabaseManager::updateLastLogin(const QString &username)
{
    QSqlQuery query;
    query.prepare("UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE username = ?");
    query.addBindValue(username);

    if (!query.exec()) {
        qWarning("Failed to update last login: %s", qPrintable(query.lastError().text()));
        return false;
    }

    return true;
}

bool DatabaseManager::addFile(const QString &username, const QString &filename, const QString &path, qint64 size, const QString &type, int directoryId)
{
    // 实现添加文件逻辑
    return false;
}

bool DatabaseManager::getFileList(const QString &username, const QString &directory, QList<QMap<QString, QVariant>> &fileList)
{
    // 1. 保证用户的根目录始终存在
    QString baseUserPath = "./files/" + username;
    QDir baseDir(baseUserPath);
    if (!baseDir.exists()) {
        baseDir.mkpath(baseUserPath);
    }

    // 2. 清理客户端传来的脏路径（去除首尾空格和斜杠）
    QString cleanDir = directory.trimmed();
    if (cleanDir.startsWith("/")) cleanDir = cleanDir.mid(1);
    if (cleanDir.endsWith("/")) cleanDir = cleanDir.left(cleanDir.length() - 1);

    // 3. 拼接目标路径
    QString targetPath = baseUserPath;
    if (!cleanDir.isEmpty()) {
        targetPath += "/" + cleanDir;
    }

    QDir targetDir(targetPath);

    // 4.如果是请求的子目录且不存在，直接返回失败,不要自动创建！
    if (!targetDir.exists()) {
        qDebug() << "Error: Target directory does not exist ->" << targetPath;
        return false;
    }

    // 5. 正常读取文件列表
    QFileInfoList entries = targetDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        QMap<QString, QVariant> file;
        file["name"] = entry.fileName();

        if (entry.isDir()) {
            file["size"] = 0;
            file["type"] = "dir";
        } else {
            file["size"] = entry.size();
            file["type"] = entry.suffix();
        }

        // 规范化返回给客户端的路径格式，例如：/files 或 /doc/test.txt
        QString outPath = "/" + cleanDir;
        if (!cleanDir.isEmpty()) {
            outPath += "/";
        }
        outPath += entry.fileName();
        file["path"] = outPath;

        file["last_modified"] = entry.lastModified().toString();
        fileList.append(file);
    }

    return true;
}

bool DatabaseManager::getDirectoryList(const QString &username, int parentId, QList<QMap<QString, QVariant>> &directoryList)
{
    // 实现获取目录列表逻辑
    return false;
}

bool DatabaseManager::updatePassword(const QString &username, const QString &passwordHash, const QString &salt)
{
    QSqlQuery query;
    query.prepare("UPDATE users SET password_hash = ?, salt = ? WHERE username = ?");
    query.addBindValue(passwordHash);
    query.addBindValue(salt);
    query.addBindValue(username);

    if (!query.exec()) {
        qWarning("Failed to update password: %s", qPrintable(query.lastError().text()));
        return false;
    }

    return true;
}

bool DatabaseManager::updateUserInfo(const QString &username, const QString &email, const QString &nickname)
{
    QSqlQuery query;
    query.prepare("UPDATE users SET email = ?, nickname = ? WHERE username = ?");
    query.addBindValue(email);
    query.addBindValue(nickname);
    query.addBindValue(username);

    if (!query.exec()) {
        qWarning("Failed to update user info: %s", qPrintable(query.lastError().text()));
        return false;
    }

    return true;
}

bool DatabaseManager::deleteUser(const QString &username)
{
    QSqlQuery query;
    
    // 获取用户ID
    query.prepare("SELECT id FROM users WHERE username = ?");
    query.addBindValue(username);
    
    if (!query.exec() || !query.next()) {
        qWarning("Failed to get user id: %s", qPrintable(query.lastError().text()));
        return false;
    }
    
    int userId = query.value(0).toInt();
    
    // 删除用户的文件记录
    query.prepare("DELETE FROM files WHERE user_id = ?");
    query.addBindValue(userId);
    query.exec();
    
    // 删除用户的目录记录
    query.prepare("DELETE FROM directories WHERE user_id = ?");
    query.addBindValue(userId);
    query.exec();
    
    // 删除用户的权限记录
    query.prepare("DELETE FROM permissions WHERE user_id = ?");
    query.addBindValue(userId);
    query.exec();
    
    // 删除用户的分享记录
    query.prepare("DELETE FROM shares WHERE user_id = ?");
    query.addBindValue(userId);
    query.exec();
    
    // 删除用户
    query.prepare("DELETE FROM users WHERE username = ?");
    query.addBindValue(username);
    
    if (!query.exec()) {
        qWarning("Failed to delete user: %s", qPrintable(query.lastError().text()));
        return false;
    }

    return true;
}
