#include "usermanager.h"
#include "databasemanager.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include "filemanager.h"

UserManager *UserManager::m_instance = nullptr;

UserManager::UserManager(QObject *parent) : QObject(parent)
{
}

UserManager::~UserManager()
{
}

UserManager *UserManager::instance()
{
    if (!m_instance) {
        m_instance = new UserManager();
    }
    return m_instance;
}

bool UserManager::registerUser(const QString &username, const QString &email, const QString &password, const QString &nickname)
{
    // 生成盐值
    QString salt = generateSalt();
    // 哈希密码
    QString passwordHash = hashPassword(password, salt);

    // 创建用户
    return DatabaseManager::instance()->createUser(username, email, passwordHash, salt, nickname);
}

bool UserManager::loginUser(const QString &username, const QString &password)
{
    QString storedPasswordHash, salt;
    if (!DatabaseManager::instance()->getUser(username, storedPasswordHash, salt)) {
        return false;
    }

    // 验证密码
    QString passwordHash = hashPassword(password, salt);
    return passwordHash == storedPasswordHash;
}

bool UserManager::changePassword(const QString &username, const QString &oldPassword, const QString &newPassword)
{
    // 验证旧密码
    if (!loginUser(username, oldPassword)) {
        return false;
    }

    // 生成新的盐值和密码哈希
    QString newSalt = generateSalt();
    QString newPasswordHash = hashPassword(newPassword, newSalt);

    // 更新密码
    return DatabaseManager::instance()->updatePassword(username, newPasswordHash, newSalt);
}

bool UserManager::updateUserInfo(const QString &username, const QString &email, const QString &nickname)
{
    return DatabaseManager::instance()->updateUserInfo(username, email, nickname);
}

bool UserManager::deleteUser(const QString &username)
{
    // 删除用户的所有文件和目录
    FileManager::instance()->deleteUserFiles(username);
    
    // 删除用户数据库记录
    return DatabaseManager::instance()->deleteUser(username);
}

QString UserManager::generateSalt()
{
    QString salt;
    for (int i = 0; i < 16; ++i) {
        salt.append(QChar('a' + QRandomGenerator::global()->bounded(26)));
    }
    return salt;
}

QString UserManager::hashPassword(const QString &password, const QString &salt)
{
    QString combined = password + salt;
    QByteArray hash = QCryptographicHash::hash(combined.toUtf8(), QCryptographicHash::Sha256);
    return hash.toHex();
}
