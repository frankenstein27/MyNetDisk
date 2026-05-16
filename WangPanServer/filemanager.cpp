#include "filemanager.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

FileManager* FileManager::m_instance = nullptr;

FileManager::FileManager(QObject* parent) : QObject(parent) {}

FileManager::~FileManager() {}

FileManager* FileManager::instance() {
    if (!m_instance) {
        m_instance = new FileManager();
    }
    return m_instance;
}

bool FileManager::init(const QString& basePath) {
    m_basePath = basePath;
    QDir dir(m_basePath);
    if (!dir.exists()) {
        return dir.mkpath(m_basePath);
    }
    return true;
}

bool FileManager::saveFile(const QString& username, const QString& filename, const QByteArray& data) {
    qDebug() << "=== FileManager::saveFile called ===";
    QString userPath = m_basePath + "/" + username;

    QDir dir(userPath);
    if (!dir.exists()) {
        if (!dir.mkpath(userPath)) {
            return false;
        }
    }

    QString filePath;
    if (filename.startsWith("/")) {
        QString relativePath = filename.mid(1);
        filePath = userPath + "/" + relativePath;

        QDir fileDir = QFileInfo(filePath).dir();
        if (!fileDir.exists()) {
            fileDir.mkpath(fileDir.path());
        }
    } else {
        filePath = userPath + "/" + filename;
    }

    // 新增：防止将数据写入到一个同名文件夹上
    if (QFileInfo(filePath).isDir()) {
        qDebug() << "Error: Target path is a directory, cannot save file:" << filePath;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open file for writing";
        return false;
    }

    qint64 written = file.write(data);
    file.close();
    return written == data.size();
}

QByteArray FileManager::readFile(const QString& username, const QString& filename) {
    QString filePath;
    if (filename.startsWith("/")) {
        QString relativePath = filename.mid(1);
        filePath = m_basePath + "/" + username + "/" + relativePath;
    } else {
        filePath = m_basePath + "/" + username + "/" + filename;
    }

    // 拦截读取文件夹的操作
    if (QFileInfo(filePath).isDir()) {
        qDebug() << "Error: Cannot read a directory as a file:" << filePath;
        return QByteArray();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }

    QByteArray data = file.readAll();
    file.close();
    return data;
}

bool FileManager::deleteFile(const QString& username, const QString& filename) {
    QString filePath;
    if (filename.startsWith("/")) {
        QString relativePath = filename.mid(1);
        filePath = m_basePath + "/" + username + "/" + relativePath;
    } else {
        filePath = m_basePath + "/" + username + "/" + filename;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) return false;

    // 如果检测到是文件夹，自动切换为递归删除文件夹逻辑
    if (fileInfo.isDir()) {
        QDir dir(filePath);
        bool result = dir.removeRecursively();
        qDebug() << "Delete directory:" << filePath << "result:" << result;
        return result;
    }

    QFile file(filePath);
    bool result = file.remove();
    qDebug() << "Delete file:" << filePath << "result:" << result;
    return result;
}

bool FileManager::renameFile(const QString& username, const QString& oldFilename, const QString& newFilename) {
    QString oldPath;
    QString newPath;

    if (oldFilename.startsWith("/")) {
        oldPath = m_basePath + "/" + username + "/" + oldFilename.mid(1);
    } else {
        oldPath = m_basePath + "/" + username + "/" + oldFilename;
    }

    if (newFilename.startsWith("/")) {
        newPath = m_basePath + "/" + username + "/" + newFilename.mid(1);
    } else {
        newPath = m_basePath + "/" + username + "/" + newFilename;
    }

    // 确保目标路径的父级目录存在，否则重命名会失败
    QDir newDir = QFileInfo(newPath).dir();
    if (!newDir.exists()) {
        newDir.mkpath(newDir.path());
    }

    QFileInfo fileInfo(oldPath);
    bool result = false;

    // 针对文件夹和文件分别使用不同的重命名类
    if (fileInfo.isDir()) {
        QDir dir;
        result = dir.rename(oldPath, newPath);
        qDebug() << "Rename from" << oldPath << "to" << newPath << "result:" << result;
    } else {
        QFile file(oldPath);
        result = file.rename(newPath);
        qDebug() << "Rename from" << oldPath << "to" << newPath << "result:" << result << "error:" << file.errorString();
    }

    return result;
}

bool FileManager::deleteDirectory(const QString& username, const QString& dirname) {
    QString dirPath;
    if (dirname.startsWith("/")) {
        dirPath = m_basePath + "/" + username + "/" + dirname.mid(1);
    } else {
        dirPath = m_basePath + "/" + username + "/" + dirname;
    }

    QDir dir(dirPath);
    if (dir.exists()) {
        return dir.removeRecursively();
    }
    return false;
}

bool FileManager::deleteUserFiles(const QString& username) {
    QString userPath = m_basePath + "/" + username;
    QDir userDir(userPath);
    if (userDir.exists()) {
        return userDir.removeRecursively();
    }
    return true;
}

bool FileManager::moveFile(const QString& username, const QString& oldPath, const QString& newPath) {
    // 移动文件/文件夹本质上与重命名逻辑一致，可直接复用
    return renameFile(username, oldPath, newPath);
}

qint64 FileManager::getFileSize(const QString& username, const QString& filename) {
    QString filePath;
    if (filename.startsWith("/")) {
        filePath = m_basePath + "/" + username + "/" + filename.mid(1);
    } else {
        filePath = m_basePath + "/" + username + "/" + filename;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return -1;
    }

    // 如果是文件夹，递归遍历计算其内部所有文件的总大小
    if (fileInfo.isDir()) {
        qint64 totalSize = 0;
        QDirIterator it(filePath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            totalSize += it.fileInfo().size();
        }
        return totalSize;
    }

    return fileInfo.size();
}

QString FileManager::getFileHash(const QString& username, const QString& filename) {
    QString filePath;
    if (filename.startsWith("/")) {
        filePath = m_basePath + "/" + username + "/" + filename.mid(1);
    } else {
        filePath = m_basePath + "/" + username + "/" + filename;
    }

    // 拦截文件夹哈希请求
    if (QFileInfo(filePath).isDir()) {
        qDebug() << "Error: Cannot calculate hash for a directory:" << filePath;
        return QString();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        file.close();
        return QString();
    }

    file.close();
    return hash.result().toHex();
}

qint64 FileManager::getUserUsedSpace(const QString& username) {
    QString userPath = m_basePath + "/" + username;
    QDir userDir(userPath);
    if (!userDir.exists()) return 0;

    qint64 totalSize = 0;
    QDirIterator it(userPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        totalSize += it.fileInfo().size();
    }
    return totalSize;
}
