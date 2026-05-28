#include "databasemanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

DatabaseManager* DatabaseManager::m_instance = nullptr;

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {}

DatabaseManager::~DatabaseManager() { disconnect(); }

DatabaseManager* DatabaseManager::instance() {
    if (!m_instance) {
        m_instance = new DatabaseManager();
    }
    return m_instance;
}

bool DatabaseManager::connect() {
    // 从配置文件读取数据库连接参数（配置文件与可执行文件同目录）
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    QString host = settings.value("Database/host", "localhost").toString();
    QString dbName = settings.value("Database/name", "wangpan").toString();
    QString user = settings.value("Database/user", "root").toString();
#ifdef Q_OS_WIN
    QString pass = settings.value("Database/password", "root").toString();
#else
    QString pass = settings.value("Database/password", "hebo").toString();
#endif

    m_db = QSqlDatabase::addDatabase("QMYSQL");
    m_db.setHostName(host);
    m_db.setDatabaseName(dbName);
    m_db.setUserName(user);
    m_db.setPassword(pass);

    if (!m_db.open()) {
        qWarning("Failed to connect to database: %s", qPrintable(m_db.lastError().text()));
        return false;
    }

    // 创建表结构
    createTables();
    return true;
}

void DatabaseManager::disconnect() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::isConnected() const { return m_db.isOpen(); }

void DatabaseManager::createTables() {
    QSqlQuery query;

    // 创建用户表
    query.exec(
        "CREATE TABLE IF NOT EXISTS users ("
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
    query.exec(
        "CREATE TABLE IF NOT EXISTS directories ("
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
    query.exec(
        "CREATE TABLE IF NOT EXISTS files ("
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

    // 创建日志表（target_id 使用 VARCHAR 因为实际存储的是文件名/目录名）
    query.exec(
        "CREATE TABLE IF NOT EXISTS logs ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "user_id INT, "
        "action VARCHAR(50) NOT NULL, "
        "target_type TINYINT, "
        "target_id VARCHAR(255),"
        "details TEXT, "
        "ip VARCHAR(50), "
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
        "FOREIGN KEY (user_id) REFERENCES users(id) "
        ")");

    // 迁移：修复已有数据库中 target_id 列类型
    query.exec("ALTER TABLE logs MODIFY COLUMN target_id VARCHAR(255)");

    // 创建服务器监控表
    query.exec(
        "CREATE TABLE IF NOT EXISTS server_monitor ("
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

bool DatabaseManager::createUser(const QString& username, const QString& email, const QString& passwordHash, const QString& salt, const QString& nickname) {
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
    query.addBindValue(QVariant());  // NULL parent_id（根目录无父级）
    query.addBindValue(userId);

    if (!query.exec()) {
        qWarning("Failed to create root directory: %s", qPrintable(query.lastError().text()));
        return false;
    }

    return true;
}

bool DatabaseManager::getUser(const QString& username, QString& passwordHash, QString& salt) {
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

bool DatabaseManager::getUserInfo(const QString& username, QString& email, QString& nickname, qint64& quota, qint64& usedSpace) {
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

bool DatabaseManager::updateLastLogin(const QString& username) {
    QSqlQuery query;
    query.prepare("UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE username = ?");
    query.addBindValue(username);

    if (!query.exec()) {
        qWarning("Failed to update last login: %s", qPrintable(query.lastError().text()));
        return false;
    }

    return true;
}

bool DatabaseManager::addFile(const QString& username, const QString& filename, const QString& path, qint64 size, const QString& type, int directoryId) {
    // 获取用户 ID（使用独立的 QSqlQuery 对象，避免与后续 INSERT 产生驱动冲突）
    QSqlQuery selQuery;
    selQuery.prepare("SELECT id FROM users WHERE username = ? AND status = 1");
    selQuery.addBindValue(username);
    if (!selQuery.exec() || !selQuery.next()) {
        qWarning("addFile: user not found: %s", qPrintable(username));
        return false;
    }
    int userId = selQuery.value(0).toInt();
    selQuery.clear();  // 显式释放结果集

    // 使用新的 QSqlQuery 对象执行 INSERT，避免 MySQL "Commands out of sync" 错误
    QSqlQuery insQuery;
    insQuery.prepare("INSERT INTO files (name, path, size, type, directory_id, user_id) VALUES (?, ?, ?, ?, ?, ?)");
    insQuery.addBindValue(filename);
    insQuery.addBindValue(path);
    insQuery.addBindValue(size);
    insQuery.addBindValue(type);
    insQuery.addBindValue(directoryId);
    insQuery.addBindValue(userId);

    if (!insQuery.exec()) {
        qWarning("addFile: insert failed: %s", qPrintable(insQuery.lastError().text()));
        return false;
    }
    return true;
}

bool DatabaseManager::getFileList(const QString& username, const QString& directory, QList<QMap<QString, QVariant>>& fileList) {
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
    for (const QFileInfo& entry : entries) {
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

bool DatabaseManager::createDirectory(const QString& username, const QString& path) {
    // 在文件系统中创建目录
    QString userPath = "./files/" + username;

    // 如果path不为空且不是根目录，则添加到路径中
    QString directoryPath = userPath;
    if (!path.isEmpty() && path != "/") {
        // 移除开头的斜杠
        QString dir = path;
        if (dir.startsWith("/")) {
            dir = dir.mid(1);
        }
        directoryPath += "/" + dir;
    }

    QDir dir;
    if (!dir.exists(directoryPath)) {
        if (dir.mkpath(directoryPath)) {
            qDebug() << "Created directory:" << directoryPath;
            return true;
        } else {
            qDebug() << "Failed to create directory:" << directoryPath;
            return false;
        }
    }

    return true;
}

bool DatabaseManager::getDirectoryList(const QString& username, int parentId, QList<QMap<QString, QVariant>>& directoryList) {
    QSqlQuery query;
    query.prepare(
        "SELECT d.id, d.name, d.parent_id, d.created_at "
        "FROM directories d "
        "JOIN users u ON d.user_id = u.id "
        "WHERE u.username = ? AND u.status = 1 "
        "AND d.parent_id " +
        (parentId == -1 ? QString("IS NULL") : QString("= ?")) + " ORDER BY d.name");
    query.addBindValue(username);
    if (parentId != -1) {
        query.addBindValue(parentId);
    }

    if (!query.exec()) {
        qWarning("getDirectoryList: query failed: %s", qPrintable(query.lastError().text()));
        return false;
    }

    while (query.next()) {
        QMap<QString, QVariant> dir;
        dir["id"] = query.value(0).toInt();
        dir["name"] = query.value(1).toString();
        dir["parent_id"] = query.value(2).isNull() ? -1 : query.value(2).toInt();
        dir["created_at"] = query.value(3).toString();
        directoryList.append(dir);
    }
    return true;
}

int DatabaseManager::getRootDirectoryId(const QString& username) {
    QSqlQuery query;
    query.prepare(
        "SELECT d.id FROM directories d "
        "JOIN users u ON d.user_id = u.id "
        "WHERE u.username = ? AND d.parent_id IS NULL");
    query.addBindValue(username);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    // 根目录记录缺失（历史用户迁移场景）：自动补齐
    qWarning("getRootDirectoryId: root dir not found for user %s, auto-creating...", qPrintable(username));
    QSqlQuery ins;
    ins.prepare(
        "INSERT INTO directories (name, parent_id, user_id) "
        "SELECT 'root', NULL, id FROM users WHERE username = ? AND status = 1");
    ins.addBindValue(username);
    if (ins.exec()) {
        int newId = ins.lastInsertId().toInt();
        if (newId > 0) {
            qWarning("getRootDirectoryId: auto-created root dir id=%d for user %s", newId, qPrintable(username));
            return newId;
        }
    }
    qWarning("getRootDirectoryId: failed to auto-create root dir for user %s", qPrintable(username));
    return -1;
}

int DatabaseManager::ensureDirectoryId(const QString& username, const QString& path) {
    // 先获取根目录 ID
    int userId = 0;
    {
        QSqlQuery q;
        q.prepare("SELECT id FROM users WHERE username = ? AND status = 1");
        q.addBindValue(username);
        if (!q.exec() || !q.next()) return -1;
        userId = q.value(0).toInt();
    }
    int rootId = getRootDirectoryId(username);
    if (rootId < 0) return -1;

    int parentId = rootId;
    QStringList parts = path.split('/', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        QSqlQuery q;
        q.prepare("SELECT id FROM directories WHERE name = ? AND user_id = ? AND parent_id = ?");
        q.addBindValue(part);
        q.addBindValue(userId);
        q.addBindValue(parentId);
        if (q.exec() && q.next()) {
            parentId = q.value(0).toInt();
        } else {
            QSqlQuery ins;
            ins.prepare("INSERT INTO directories (name, parent_id, user_id) VALUES (?, ?, ?)");
            ins.addBindValue(part);
            ins.addBindValue(parentId);
            ins.addBindValue(userId);
            if (!ins.exec()) {
                qWarning("ensureDirectoryId: failed to create dir %s", qPrintable(part));
                return -1;
            }
            parentId = ins.lastInsertId().toInt();
        }
    }
    return parentId;
}

void DatabaseManager::logAction(const QString& username, const QString& action, const QString& targetType, const QString& targetId, const QString& details, const QString& ip) {
    // 使用独立的 QSqlQuery 对象查询用户 ID
    QSqlQuery selQuery;
    selQuery.prepare("SELECT id FROM users WHERE username = ? AND status = 1");
    selQuery.addBindValue(username);
    int userId = 0;
    if (selQuery.exec() && selQuery.next()) {
        userId = selQuery.value(0).toInt();
    }
    selQuery.clear();  // 显式释放结果集

    // 使用新的 QSqlQuery 对象执行 INSERT
    QSqlQuery insQuery;
    insQuery.prepare("INSERT INTO logs (user_id, action, target_type, target_id, details, ip) VALUES (?, ?, ?, ?, ?, ?)");
    insQuery.addBindValue(userId > 0 ? userId : QVariant());
    insQuery.addBindValue(action);
    insQuery.addBindValue(targetType == "file" ? 1 : (targetType == "directory" ? 2 : 0));
    insQuery.addBindValue(targetId);
    insQuery.addBindValue(details);
    insQuery.addBindValue(ip);
    if (!insQuery.exec()) {
        qWarning("logAction: insert failed: %s", qPrintable(insQuery.lastError().text()));
    }
}

bool DatabaseManager::updatePassword(const QString& username, const QString& passwordHash, const QString& salt) {
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

bool DatabaseManager::updateUserInfo(const QString& username, const QString& email, const QString& nickname) {
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

bool DatabaseManager::updateUserAvatar(const QString& username, const QString& avatarPath) {
    QSqlQuery query;
    query.prepare("UPDATE users SET avatar = ? WHERE username = ?");
    query.addBindValue(avatarPath);
    query.addBindValue(username);

    if (!query.exec()) {
        qWarning("Failed to update avatar: %s", qPrintable(query.lastError().text()));
        return false;
    }
    return true;
}

void DatabaseManager::updateUsedSpace(const QString& username) {
    // 由 FileManager 计算实际已用空间并写入数据库
    // 注意：此函数不负责计算，仅更新字段；计算在调用方完成
}

bool DatabaseManager::deleteUser(const QString& username) {
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
