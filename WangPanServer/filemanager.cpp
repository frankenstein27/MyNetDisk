#include "filemanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDebug>

FileManager *FileManager::m_instance = nullptr;

FileManager::FileManager(QObject *parent) : QObject(parent)
{
}

FileManager::~FileManager()
{
}

FileManager *FileManager::instance()
{
    if (!m_instance) {
        m_instance = new FileManager();
    }
    return m_instance;
}

bool FileManager::init(const QString &basePath)
{
    m_basePath = basePath;
    QDir dir(m_basePath);
    if (!dir.exists()) {
        return dir.mkpath(m_basePath);
    }
    return true;
}

bool FileManager::saveFile(const QString &username, const QString &filename, const QByteArray &data)
{
    qDebug() << "=== FileManager::saveFile called ===";
    qDebug() << "username:" << username;
    qDebug() << "filename:" << filename;
    qDebug() << "data size:" << data.size();
    
    QString userPath = m_basePath + "/" + username;
    qDebug() << "userPath:" << userPath;
    
    QDir dir(userPath);
    if (!dir.exists()) {
        if (!dir.mkpath(userPath)) {
            return false;
        }
    }

    QString filePath;
    
    // 如果filename包含路径（以/开头），则直接使用
    if (filename.startsWith("/")) {
        // 移除开头的斜杠
        QString relativePath = filename.mid(1);
        filePath = userPath + "/" + relativePath;
        
        // 确保目录存在
        QDir fileDir = QFileInfo(filePath).dir();
        if (!fileDir.exists()) {
            fileDir.mkpath(fileDir.path());
        }
    } else {
        filePath = userPath + "/" + filename;
    }
    
    qDebug() << "filePath:" << filePath;
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open file for writing";
        return false;
    }

    qint64 written = file.write(data);
    file.close();
    qDebug() << "written bytes:" << written;
    return written == data.size();
}

QByteArray FileManager::readFile(const QString &username, const QString &filename)
{
    QString filePath;
    
    // 如果filename包含路径（以/开头），则直接使用
    if (filename.startsWith("/")) {
        // 移除开头的斜杠
        QString relativePath = filename.mid(1);
        filePath = m_basePath + "/" + username + "/" + relativePath;
    } else {
        filePath = m_basePath + "/" + username + "/" + filename;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file:" << filePath;
        return QByteArray();
    }

    QByteArray data = file.readAll();
    file.close();
    qDebug() << "Read file:" << filePath << "size:" << data.size();
    return data;
}

bool FileManager::deleteFile(const QString &username, const QString &filename)
{
    QString filePath;
    
    // 如果filename包含路径（以/开头），则直接使用
    if (filename.startsWith("/")) {
        // 移除开头的斜杠
        QString relativePath = filename.mid(1);
        filePath = m_basePath + "/" + username + "/" + relativePath;
    } else {
        filePath = m_basePath + "/" + username + "/" + filename;
    }
    
    QFile file(filePath);
    bool result = file.remove();
    qDebug() << "Delete file:" << filePath << "result:" << result;
    return result;
}

bool FileManager::renameFile(const QString &username, const QString &oldFilename, const QString &newFilename)
{
    QString oldPath;
    QString newPath;
    
    // 如果oldFilename包含路径（以/开头），则直接使用
    if (oldFilename.startsWith("/")) {
        QString relativeOldPath = oldFilename.mid(1);
        oldPath = m_basePath + "/" + username + "/" + relativeOldPath;
    } else {
        oldPath = m_basePath + "/" + username + "/" + oldFilename;
    }
    
    // 如果newFilename包含路径（以/开头），则直接使用
    if (newFilename.startsWith("/")) {
        QString relativeNewPath = newFilename.mid(1);
        newPath = m_basePath + "/" + username + "/" + relativeNewPath;
    } else {
        newPath = m_basePath + "/" + username + "/" + newFilename;
    }
    
    QFile file(oldPath);
    bool result = file.rename(newPath);
    qDebug() << "Rename file from" << oldPath << "to" << newPath << "result:" << result;
    return result;
}

bool FileManager::deleteDirectory(const QString &username, const QString &dirname)
{
    QString dirPath;
    
    // 如果dirname包含路径（以/开头），则直接使用
    if (dirname.startsWith("/")) {
        QString relativePath = dirname.mid(1);
        dirPath = m_basePath + "/" + username + "/" + relativePath;
    } else {
        dirPath = m_basePath + "/" + username + "/" + dirname;
    }
    
    QDir dir(dirPath);
    if (dir.exists()) {
        return dir.removeRecursively();
    }
    return false;
}

bool FileManager::deleteUserFiles(const QString &username)
{
    QString userPath = m_basePath + "/" + username;
    QDir userDir(userPath);
    if (userDir.exists()) {
        return userDir.removeRecursively();
    }
    return true; // 如果目录不存在，也返回成功
}

bool FileManager::moveFile(const QString &username, const QString &oldPath, const QString &newPath)
{
    // 实现移动文件逻辑
    return false;
}

qint64 FileManager::getFileSize(const QString &username, const QString &filename)
{
    QString filePath;
    
    // 如果filename包含路径（以/开头），则直接使用
    if (filename.startsWith("/")) {
        QString relativePath = filename.mid(1);
        filePath = m_basePath + "/" + username + "/" + relativePath;
    } else {
        filePath = m_basePath + "/" + username + "/" + filename;
    }
    
    QFile file(filePath);
    if (!file.exists()) {
        return -1;
    }
    return file.size();
}

QString FileManager::getFileHash(const QString &username, const QString &filename)
{
    QString filePath;
    
    // 如果filename包含路径（以/开头），则直接使用
    if (filename.startsWith("/")) {
        QString relativePath = filename.mid(1);
        filePath = m_basePath + "/" + username + "/" + relativePath;
    } else {
        filePath = m_basePath + "/" + username + "/" + filename;
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
