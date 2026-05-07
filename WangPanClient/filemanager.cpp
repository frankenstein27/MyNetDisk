#include "filemanager.h"
#include "networkmanager.h"

FileManager *FileManager::m_instance = nullptr;

FileManager::FileManager(QObject *parent) : QObject(parent)
{
    // 连接网络管理器的信号
    connect(NetworkManager::instance(), &NetworkManager::fileListReceived, this, [=](const QByteArray &data) {
        // 解析文件列表数据
        m_fileList.clear();
        QStringList items = QString(data).split('\t');
        for (const QString &item : items) {
            if (item.isEmpty()) continue;
            QStringList parts = item.split('|');
            if (parts.size() >= 5) {
                QString name = parts[0];
                qint64 size = parts[1].toLongLong();
                QString type = parts[2];
                QString path = parts[3];
                QString modifyTime = parts[4];
                m_fileList.append(FileInfo(name, size, type, path, modifyTime));
            }
        }
        emit fileListUpdated();
    });

    // 连接上传和下载进度信号
    connect(NetworkManager::instance(), &NetworkManager::uploadProgress, this, &FileManager::uploadProgress);
    connect(NetworkManager::instance(), &NetworkManager::downloadProgress, this, &FileManager::downloadProgress);
    
    // 连接上传和下载结果信号
    connect(NetworkManager::instance(), &NetworkManager::uploadResult, this, &FileManager::handleUploadResult);
    connect(NetworkManager::instance(), &NetworkManager::downloadResult, this, &FileManager::handleDownloadResult);
    connect(NetworkManager::instance(), &NetworkManager::deleteResult, this, &FileManager::handleDeleteResult);
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

QList<FileInfo> FileManager::getFileList(const QString &directory)
{
    NetworkManager::instance()->listFiles(directory);
    return m_fileList;
}

QList<FileInfo> FileManager::getFileList()
{
    return m_fileList;
}

bool FileManager::uploadFile(const QString &localPath, const QString &remotePath)
{
    return NetworkManager::instance()->uploadFile(localPath, remotePath);
}

bool FileManager::downloadFile(const QString &remotePath, const QString &localPath)
{
    return NetworkManager::instance()->downloadFile(remotePath, localPath);
}

bool FileManager::createDirectory(const QString &path)
{
    if (!NetworkManager::instance()->isConnected()) {
        return false;
    }

    // 发送创建目录请求
    QByteArray request;
    request.append("MKDIR " + path.toUtf8() + "\n");
    NetworkManager::instance()->socket()->write(request);
    if (!NetworkManager::instance()->socket()->waitForBytesWritten()) {
        return false;
    }

    // 读取响应
    QByteArray response = NetworkManager::instance()->socket()->readLine();
    QString line = QString::fromUtf8(response);
    return line.startsWith("MKDIR_OK");
}

bool FileManager::deleteFile(const QString &path)
{
    if (!NetworkManager::instance()->isConnected()) {
        return false;
    }

    // 发送删除文件请求
    QByteArray request;
    request.append("DELETE " + path.toUtf8() + "\n");
    NetworkManager::instance()->socket()->write(request);
    return NetworkManager::instance()->socket()->waitForBytesWritten();
}

bool FileManager::renameFile(const QString &oldPath, const QString &newPath)
{
    if (!NetworkManager::instance()->isConnected()) {
        return false;
    }

    // 发送重命名文件请求
    QByteArray request;
    request.append("RENAME " + oldPath.toUtf8() + " " + newPath.toUtf8() + "\n");
    NetworkManager::instance()->socket()->write(request);
    if (!NetworkManager::instance()->socket()->waitForBytesWritten()) {
        return false;
    }

    // 读取响应
    QByteArray response = NetworkManager::instance()->socket()->readLine();
    QString line = QString::fromUtf8(response);
    return line.startsWith("RENAME_OK");
}

void FileManager::handleUploadResult(bool success, const QString &message)
{
    // 转发上传结果信号
    emit uploadResult(success, message);
    
    // 如果上传成功，刷新文件列表
    if (success) {
        // 刷新当前目录的文件列表
        NetworkManager::instance()->listFiles("");
    }
}

void FileManager::handleDownloadResult(bool success, const QString &message)
{
    // 转发下载结果信号
    emit downloadResult(success, message);
}

void FileManager::handleDeleteResult(bool success, const QString &message)
{
    // 转发删除结果信号
    emit deleteResult(success, message);
}
