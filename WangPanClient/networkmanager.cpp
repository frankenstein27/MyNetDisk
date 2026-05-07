#include "networkmanager.h"
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QDebug>
NetworkManager *NetworkManager::m_instance = nullptr;

NetworkManager::NetworkManager(QObject *parent) : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    m_serverAddress = "127.0.0.1";
    m_serverPort = 8888;
    m_isDownloading = false;
    m_downloadFile = nullptr;
    m_downloadFileSize = 0;
    m_downloadBytesReceived = 0;

    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
    //connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkManager::onError);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
}

NetworkManager::~NetworkManager()
{
    delete m_socket;
}

NetworkManager *NetworkManager::instance()
{
    if (!m_instance) {
        m_instance = new NetworkManager();
    }
    return m_instance;
}

bool NetworkManager::connectToServer()
{
    m_socket->connectToHost(m_serverAddress, m_serverPort);
    return m_socket->waitForConnected(5000);
}

void NetworkManager::disconnectFromServer()
{
    m_socket->disconnectFromHost();
}

bool NetworkManager::isConnected() const
{
    return m_socket->state() == QTcpSocket::ConnectedState;
}

bool NetworkManager::login(const QString &username, const QString &password)
{
    if (!isConnected()) {
        emit error("未连接到服务器");
        return false;
    }

    // 构建登录请求
    QByteArray request;
    request.append("LOGIN " + username.toUtf8() + " " + password.toUtf8() + "\n");
    m_socket->write(request);
    return m_socket->waitForBytesWritten();
}

bool NetworkManager::registerUser(const QString &username, const QString &email, const QString &password, const QString &nickname)
{
    if (!isConnected()) {
        emit error("未连接到服务器");
        return false;
    }

    // 构建注册请求
    QByteArray request;
    request.append("REGISTER " + username.toUtf8() + " " + email.toUtf8() + " " + password.toUtf8() + " " + nickname.toUtf8() + "\n");
    m_socket->write(request);
    return m_socket->waitForBytesWritten();
}

bool NetworkManager::uploadFile(const QString &localPath, const QString &remotePath)
{
    if (!isConnected()) {
        emit error("未连接到服务器");
        return false;
    }

    // 打开本地文件
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit error("无法打开本地文件");
        return false;
    }

    qint64 fileSize = file.size();
    QString fileName = QFileInfo(file).fileName();
    
    qDebug() << "=== uploadFile called ===";
    qDebug() << "localPath:" << localPath;
    qDebug() << "remotePath:" << remotePath;
    qDebug() << "fileName:" << fileName;
    qDebug() << "fileSize:" << fileSize;

    // 发送上传请求（不使用断点续传，简化逻辑）
    QByteArray request;
    qint64 offset = 0;
    request.append("UPLOAD " + remotePath.toUtf8() + " " + QString::number(fileSize).toUtf8() + " " + fileName.toUtf8() + " " + QString::number(offset).toUtf8() + "\n");
    qDebug() << "UPLOAD request:" << request;
    m_socket->write(request);
    if (!m_socket->waitForBytesWritten()) {
        qDebug() << "Failed to write UPLOAD request";
        file.close();
        return false;
    }
    qDebug() << "UPLOAD request sent successfully";

    // 等待服务器准备接收数据
    QThread::msleep(100);

    // 分块上传文件
    const int chunkSize = 1024 * 1024; // 1MB
    char buffer[chunkSize];
    qint64 bytesSent = offset;
    qint64 bytesToSend = fileSize - offset;

    qDebug() << "Uploading file:" << fileName;
    qDebug() << "File size:" << fileSize << "Offset:" << offset;
    qDebug() << "Bytes to send:" << bytesToSend;
    qDebug() << "File at end:" << file.atEnd();
    qDebug() << "File position:" << file.pos();

    while (!file.atEnd() && bytesSent < fileSize) {
        qint64 bytesRead = file.read(buffer, chunkSize);
        qDebug() << "Read" << bytesRead << "bytes from file";
        if (bytesRead > 0) {
            // 确保不发送超过剩余的数据量
            if (bytesSent + bytesRead > fileSize) {
                bytesRead = fileSize - bytesSent;
                qDebug() << "Adjusted bytesRead to" << bytesRead;
            }

            qint64 bytesWritten = m_socket->write(buffer, bytesRead);
            qDebug() << "Wrote" << bytesWritten << "bytes to socket";
            if (bytesWritten != bytesRead) {
                qDebug() << "Error: bytes written != bytes read";
            }
            bytesSent += bytesRead;
            emit uploadProgress(bytesSent, fileSize);
            qDebug() << "Sent:" << bytesSent << "/" << fileSize;
        } else if (bytesRead == -1) {
            qDebug() << "Error reading from file";
            break;
        }
    }

    qDebug() << "Upload completed: sent" << bytesSent << "bytes";
    qDebug() << "File at end after upload:" << file.atEnd();
    qDebug() << "Final file position:" << file.pos();

    file.close();
    
    qDebug() << "Upload finished, waiting for server response";
    
    // 响应会由onReadyRead函数处理，不需要在这里等待
    return true;
}

bool NetworkManager::downloadFile(const QString &remotePath, const QString &localPath)
{
    if (!isConnected()) {
        emit error("未连接到服务器");
        return false;
    }

    // 如果已经在下载，返回错误
    if (m_isDownloading) {
        emit error("正在下载其他文件");
        return false;
    }

    // 设置下载标志
    m_isDownloading = true;
    m_downloadPath = localPath;
    m_downloadBytesReceived = 0;

    // 打开本地文件
    m_downloadFile = new QFile(localPath);
    if (m_downloadFile->exists()) {
        if (!m_downloadFile->open(QIODevice::Append)) {
            emit error("无法打开本地文件");
            delete m_downloadFile;
            m_downloadFile = nullptr;
            m_isDownloading = false;
            return false;
        }
        m_downloadBytesReceived = m_downloadFile->size();
    } else {
        if (!m_downloadFile->open(QIODevice::WriteOnly)) {
            emit error("无法创建本地文件");
            delete m_downloadFile;
            m_downloadFile = nullptr;
            m_isDownloading = false;
            return false;
        }
    }

    // 发送下载请求
    QByteArray request;
    request.append("DOWNLOAD " + remotePath.toUtf8() + " " + QString::number(m_downloadBytesReceived).toUtf8() + "\n");
    m_socket->write(request);
    m_socket->flush();
    
    return true;
}

bool NetworkManager::listFiles(const QString &directory)
{
    if (!isConnected()) {
        emit error("未连接到服务器");
        return false;
    }

    // 构建列出文件请求
    QByteArray request;
    request.append("LIST " + directory.toUtf8() + "\n");
    m_socket->write(request);
    return m_socket->waitForBytesWritten();
}

void NetworkManager::onConnected()
{
    emit connected();
}

void NetworkManager::onDisconnected()
{
    emit disconnected();
}

void NetworkManager::onError(QAbstractSocket::SocketError error)
{
    //emit error(m_socket->errorString());
}

void NetworkManager::onReadyRead()
{
    if (m_isDownloading && m_downloadFile) {
            if (m_downloadFileSize == 0) {
                // 必须等待有一整行数据到来
                if (!m_socket->canReadLine()) return;

                // 使用 trimmed() 彻底清除 \r\n 等不可见字符的干扰
                QString line = QString::fromUtf8(m_socket->readLine()).trimmed();

                if (line.startsWith("DOWNLOAD_OK")) {
                    m_downloadFileSize = line.mid(11).trimmed().toLongLong();
                } else if (line.startsWith("DOWNLOAD_FAIL")) {
                    emit downloadResult(false, line.mid(13));
                    m_downloadFile->close();
                    delete m_downloadFile;
                    m_downloadFile = nullptr;
                    m_isDownloading = false;
                    return;
                }
            }

            if (m_downloadFileSize > 0 && m_socket->bytesAvailable() > 0) {
                QByteArray data = m_socket->readAll();
                m_downloadFile->write(data);
                m_downloadBytesReceived += data.size();

                if (m_downloadBytesReceived >= m_downloadFileSize) {
                    m_downloadFile->flush(); // 强制刷入磁盘
                    m_downloadFile->close(); // 彻底释放系统文件锁
                    delete m_downloadFile;
                    m_downloadFile = nullptr;

                    // 重置状态并在最后一步发送信号，防止预览窗口提前抢占文件
                    m_isDownloading = false;
                    m_downloadFileSize = 0;
                    m_downloadBytesReceived = 0;
                    emit downloadResult(true, "下载成功");
                }
            }
            return;
        }

    m_buffer.append(m_socket->readAll());

    // 处理服务器响应
    while (m_buffer.contains('\n')) {
        int pos = m_buffer.indexOf('\n');
        QByteArray line = m_buffer.left(pos);
        m_buffer.remove(0, pos + 1);

        // 解析响应
        if (line.startsWith("LOGIN_OK")) {
            emit loginResult(true, "登录成功");
        } else if (line.startsWith("LOGIN_FAIL")) {
            QString message = QString::fromUtf8(line.mid(10));
            emit loginResult(false, message);
        } else if (line.startsWith("REGISTER_OK")) {
            emit registerResult(true, "注册成功");
        } else if (line.startsWith("REGISTER_FAIL")) {
            QString message = QString::fromUtf8(line.mid(13));
            emit registerResult(false, message);
        } else if (line.startsWith("FILE_LIST")) {
            emit fileListReceived(line.mid(10));
        } else if (line.startsWith("UPLOAD_OK")) {
            emit uploadResult(true, "上传成功");
        } else if (line.startsWith("UPLOAD_FAIL")) {
            QString message = QString::fromUtf8(line.mid(11));
            emit uploadResult(false, message);
        } else if (line.startsWith("DOWNLOAD_OK")) {
            // 下载响应在下载处理逻辑中处理
        } else if (line.startsWith("DOWNLOAD_FAIL")) {
            QString message = QString::fromUtf8(line.mid(13));
            emit downloadResult(false, message);
        } else if (line.startsWith("DELETE_OK")) {
            emit deleteResult(true, "删除成功");
        } else if (line.startsWith("DELETE_FAIL")) {
            QString message = QString::fromUtf8(line.mid(11));
            emit deleteResult(false, message);
        }
    }
}
