#include "networkmanager.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QUrl>

// 协议字段百分号编码（处理空格等特殊字符）
static QString encodeField(const QString& field) { return QString::fromUtf8(QUrl::toPercentEncoding(field)); }

// 客户端侧密码哈希（SHA-256），避免明文传输
static QString hashPassword(const QString& password) { return QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex(); }
NetworkManager* NetworkManager::m_instance = nullptr;

NetworkManager::NetworkManager(QObject* parent) : QObject(parent) {
    m_socket = new QTcpSocket(this);

    // 从配置文件读取服务器地址和端口（配置文件与可执行文件同目录）
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat, this);
    m_serverAddress = settings.value("Server/address", "127.0.0.1").toString();
    m_serverPort = settings.value("Server/port", 8888).toInt();

    m_isDownloading = false;
    m_isUploading = false;
    m_downloadFile = nullptr;
    m_downloadFileSize = 0;
    m_downloadBytesReceived = 0;
    m_uploadFile = nullptr;
    m_uploadFileSize = 0;
    m_uploadBytesSent = 0;
    m_isCheckingUpload = false;

    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkManager::onError);
#else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &NetworkManager::onError);
#endif
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
}

NetworkManager::~NetworkManager() { delete m_socket; }

NetworkManager* NetworkManager::instance() {
    if (!m_instance) {
        m_instance = new NetworkManager();
    }
    return m_instance;
}

bool NetworkManager::connectToServer() {
    m_socket->connectToHost(m_serverAddress, m_serverPort);
    return m_socket->waitForConnected(5000);
}

void NetworkManager::disconnectFromServer() { m_socket->disconnectFromHost(); }

bool NetworkManager::isConnected() const { return m_socket->state() == QTcpSocket::ConnectedState; }

void NetworkManager::login(const QString& username, const QString& password) {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return;
    }

    // 构建登录请求（密码做 SHA-256 哈希，避免明文传输）
    QByteArray request;
    // 登录指令格式：LOGIN username password
    request.append("LOGIN " + encodeField(username).toUtf8() + " " + hashPassword(password).toUtf8() + "\n");
    m_socket->write(request);
    m_socket->flush();
}

void NetworkManager::registerUser(const QString& username, const QString& email, const QString& password, const QString& nickname) {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return;
    }

    // 构建注册请求（密码做 SHA-256 哈希，避免明文传输）
    QByteArray request;
    // 注册指令格式：REGISTER username email password nickname
    request.append("REGISTER " + encodeField(username).toUtf8() + " " + encodeField(email).toUtf8() + " " + hashPassword(password).toUtf8() + " " + encodeField(nickname).toUtf8() + "\n");
    m_socket->write(request);
    m_socket->flush();
}

void NetworkManager::changePassword(const QString& oldPassword, const QString& newPassword, const QString& confirmPassword) {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return;
    }
    QByteArray request;
    // 更改密码指令格式：CHANGE_PASSWORD oldPassword newPassword confirmPassword（密码都做 SHA-256 哈希，避免明文传输）
    request.append("CHANGE_PASSWORD " + encodeField(hashPassword(oldPassword)).toUtf8() + " " + encodeField(hashPassword(newPassword)).toUtf8() + " " +
                   encodeField(hashPassword(confirmPassword)).toUtf8() + "\n");
    m_socket->write(request);
    m_socket->flush();
}

void NetworkManager::deleteUser() {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return;
    }
    m_socket->write("DELETE_USER\n");
    m_socket->flush();
}

bool NetworkManager::uploadFile(const QString& localPath, const QString& remotePath) {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return false;
    }

    if (m_isUploading || m_isCheckingUpload) {
        emit error("正在上传其他文件");
        return false;
    }

    QString fileName = QFileInfo(localPath).fileName();

    // 第一阶段：发送 UPLOAD_CHECK 查询服务器文件状态
    m_isCheckingUpload = true;
    m_checkLocalPath = localPath;
    m_checkRemotePath = remotePath;
    m_checkFileName = fileName;

    qDebug() << "=== UPLOAD_CHECK phase ===";
    qDebug() << "localPath:" << localPath << "remotePath:" << remotePath << "fileName:" << fileName;

    QByteArray checkRequest;
    checkRequest.append("UPLOAD_CHECK " + encodeField(remotePath).toUtf8() + " " + encodeField(fileName).toUtf8() + "\n");
    m_socket->write(checkRequest);

    // 响应 UPLOAD_RESUME / UPLOAD_NEW 由 onReadyRead 处理
    return true;
}

// 第二阶段：收到服务器响应后，发送 UPLOAD 头并开始传数据
void NetworkManager::startUploadTransfer(qint64 offset) {
    m_isCheckingUpload = false;

    m_uploadFile = new QFile(m_checkLocalPath);
    if (!m_uploadFile->open(QIODevice::ReadOnly)) {
        emit error("无法打开本地文件");
        delete m_uploadFile;
        m_uploadFile = nullptr;
        return;
    }

    m_uploadFileSize = m_uploadFile->size();
    m_uploadBytesSent = offset;
    m_uploadRemotePath = m_checkRemotePath;
    m_isUploading = true;

    qDebug() << "=== startUploadTransfer ===";
    qDebug() << "offset:" << offset << "fileSize:" << m_uploadFileSize;

    // 如果偏移量超过文件大小，视为已完成
    if (offset >= m_uploadFileSize) {
        qDebug() << "File already fully uploaded";
        m_uploadFile->close();
        delete m_uploadFile;
        m_uploadFile = nullptr;
        m_isUploading = false;
        emit uploadResult(true, "文件已存在");
        return;
    }

    // 定位到续传位置（断点续传核心）
    if (offset > 0) {
        m_uploadFile->seek(offset);
    }

    // 发送 UPLOAD 请求头
    QByteArray request;
    // 断点续传上传文件指令格式：UPLOAD remotePath fileSize fileName offset
    request.append("UPLOAD " + encodeField(m_checkRemotePath).toUtf8() + " " + QString::number(m_uploadFileSize).toUtf8() + " " + encodeField(m_checkFileName).toUtf8() + " " +
                   QString::number(offset).toUtf8() + "\n");
    qDebug() << "UPLOAD request:" << request;
    m_socket->write(request);

    if (!m_socket->waitForBytesWritten(5000)) {
        qDebug() << "Failed to write UPLOAD request";
        m_uploadFile->close();
        delete m_uploadFile;
        m_uploadFile = nullptr;
        m_isUploading = false;
        return;
    }

    qDebug() << "UPLOAD request sent, starting async data transfer";

    // 启动异步分块上传
    QTimer::singleShot(50, this, &NetworkManager::sendNextChunk);
}

void NetworkManager::sendNextChunk() {
    if (!m_isUploading || !m_uploadFile) return;

    // 防止发送缓冲区膨胀：如果待发送数据超过 1MB，等待缓冲区排空
    if (m_socket->bytesToWrite() > 1024 * 1024) {
        QTimer::singleShot(50, this, &NetworkManager::sendNextChunk);
        return;
    }

    const int chunkSize = 64 * 1024;  // 64KB 每块
    QByteArray chunk = m_uploadFile->read(chunkSize);

    if (chunk.isEmpty()) {
        // 文件数据发送完毕
        qDebug() << "Upload data transfer complete: sent" << m_uploadBytesSent << "bytes";
        m_uploadFile->close();
        delete m_uploadFile;
        m_uploadFile = nullptr;
        m_isUploading = false;
        // 服务器响应 UPLOAD_OK / UPLOAD_FAIL 由 onReadyRead 处理
        return;
    }

    m_socket->write(chunk);
    m_uploadBytesSent += chunk.size();
    emit uploadProgress(m_uploadBytesSent, m_uploadFileSize);

    // 让出事件循环，调度下一块
    QTimer::singleShot(0, this, &NetworkManager::sendNextChunk);
}

bool NetworkManager::downloadFile(const QString& remotePath, const QString& localPath) {
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

    // 清理可能残留的旧下载文件对象
    delete m_downloadFile;
    m_downloadFile = nullptr;

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
    // 下载文件指令格式：DOWNLOAD remotePath offset
    request.append("DOWNLOAD " + encodeField(remotePath).toUtf8() + " " + QString::number(m_downloadBytesReceived).toUtf8() + "\n");
    m_socket->write(request);
    m_socket->flush();

    return true;
}

bool NetworkManager::listFiles(const QString& directory) {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return false;
    }

    // 构建列出文件请求
    QByteArray request;
    // 获取文件列表指令格式：LIST directory
    request.append("LIST " + encodeField(directory).toUtf8() + "\n");
    m_socket->write(request);
    return m_socket->waitForBytesWritten();
}

void NetworkManager::copyFile(const QString& sourcePath, const QString& targetPath) {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return;
    }
    QByteArray request;
    // 复制文件/目录指令格式：COPY sourcePath targetPath
    request.append("COPY " + encodeField(sourcePath).toUtf8() + " " + encodeField(targetPath).toUtf8() + "\n");
    m_socket->write(request);
    m_socket->flush();
}

void NetworkManager::moveFile(const QString& sourcePath, const QString& targetPath) {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return;
    }
    QByteArray request;
    // 移动/粘贴文件/目录指令格式：MOVE sourcePath targetPath
    request.append("MOVE " + encodeField(sourcePath).toUtf8() + " " + encodeField(targetPath).toUtf8() + "\n");
    m_socket->write(request);
    m_socket->flush();
}

void NetworkManager::getUserInfo() {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return;
    }
    m_socket->write("GET_USER_INFO\n");
    m_socket->flush();
}

void NetworkManager::updateNickname(const QString& nickname) {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return;
    }
    QByteArray request;
    // 修改昵称指令格式：UPDATE_NICKNAME nickname
    request.append("UPDATE_NICKNAME " + encodeField(nickname).toUtf8() + "\n");
    m_socket->write(request);
    m_socket->flush();
}

void NetworkManager::updateEmail(const QString& email) {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return;
    }
    QByteArray request;
    // 修改邮箱指令格式：UPDATE_EMAIL email
    request.append("UPDATE_EMAIL " + encodeField(email).toUtf8() + "\n");
    m_socket->write(request);
    m_socket->flush();
}

void NetworkManager::updateAvatar(const QByteArray& imageData) {
    if (!isConnected()) {
        emit error("未连接到服务器");
        return;
    }
    QByteArray request;
    // 修改头像指令格式：UPDATE_AVATAR avatarData（Base64编码后的图片数据）
    request.append("UPDATE_AVATAR " + imageData.toBase64() + "\n");
    m_socket->write(request);
    m_socket->flush();
}

void NetworkManager::onConnected() { emit connected(); }

void NetworkManager::onDisconnected() { emit disconnected(); }

void NetworkManager::onError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    emit NetworkManager::error(m_socket->errorString());
}

void NetworkManager::onReadyRead() {
    if (m_isDownloading && m_downloadFile) {
        // 尝试读取指令头，使用 while 循环防止多条指令粘连
        while (m_downloadFileSize == 0 && m_socket->canReadLine()) {
            QByteArray rawLine = m_socket->readLine();
            QString line = QString::fromUtf8(rawLine).trimmed();

            if (line.startsWith("DOWNLOAD_OK")) {
                m_downloadFileSize = line.mid(11).trimmed().toLongLong();

                // 瞬间完成或空文件的情况
                if (m_downloadFileSize == 0 || m_downloadBytesReceived >= m_downloadFileSize) {
                    m_downloadFile->flush();
                    m_downloadFile->close();
                    delete m_downloadFile;
                    m_downloadFile = nullptr;
                    m_isDownloading = false;
                    emit downloadResult(true, "下载成功");
                    break;  // 跳出循环，让下方的 m_buffer 处理可能剩下的普通指令
                }
            } else if (line.startsWith("DOWNLOAD_FAIL")) {
                m_downloadFile->close();
                delete m_downloadFile;
                m_downloadFile = nullptr;
                m_isDownloading = false;
                emit downloadResult(false, line.mid(13));
                return;
            } else {
                // 如果服务器在发送文件前发来了其他指令（上一次的遗留指令），存入缓冲池，不能丢弃
                m_buffer.append(rawLine);
            }
        }

        // 接收文件实体数据
        // 正在下载 && 要下载的文件大小 ＞ 0 && socket 数据就绪，可以读取
        if (m_isDownloading && m_downloadFileSize > 0 && m_socket->bytesAvailable() > 0) {
            // 还需要读取的大小 = 需要读取的总大小 - 上次读取指令头的大小
            qint64 bytesToRead = m_downloadFileSize - m_downloadBytesReceived;
            // 从 socket 读取数据
            QByteArray data = m_socket->read(bytesToRead);
            // 写入到文件中
            m_downloadFile->write(data);
            // 更新已下载的数据大小
            m_downloadBytesReceived += data.size();
            // 更新进度条
            emit downloadProgress(m_downloadBytesReceived, m_downloadFileSize);

            // 如果 已接收的文件大小 ＞= 总要接收的文件大小，代表下载完成
            if (m_downloadBytesReceived >= m_downloadFileSize) {
                // 清理工作和UI更新
                m_downloadFile->flush();
                m_downloadFile->close();
                delete m_downloadFile;
                m_downloadFile = nullptr;

                // 更新下载状态
                m_isDownloading = false;
                m_downloadFileSize = 0;
                m_downloadBytesReceived = 0;
                emit downloadResult(true, "下载成功");  // 明确发送成功信号
            }
        }

        // 如果还在下载中，说明当前包读完了，直接返回等待下一个包
        if (m_isDownloading) {
            return;
        }
    }

    // 处理非下载状态（或在下载过程中加入缓冲池）的指令
    m_buffer.append(m_socket->readAll());

    while (m_buffer.contains('\n')) {
        int pos = m_buffer.indexOf('\n');
        QByteArray line = m_buffer.left(pos);
        m_buffer.remove(0, pos + 1);

        // 解析响应（根据响应头判断类型，提取信息并发出对应信号，再由信号对应的槽函数处理）
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
            // 返回下标为10及以后的值
            emit fileListReceived(line.mid(10));
        } else if (line.startsWith("UPLOAD_RESUME")) {
            // 服务器告知已有文件大小，从该偏移续传
            if (m_isCheckingUpload) {
                qint64 offset = line.mid(14).trimmed().toLongLong();
                qDebug() << "UPLOAD_RESUME: offset =" << offset;
                startUploadTransfer(offset);
            }
        } else if (line.startsWith("UPLOAD_NEW")) {
            // 服务器告知无此文件，从头开始上传
            if (m_isCheckingUpload) {
                qDebug() << "UPLOAD_NEW: starting from offset 0";
                startUploadTransfer(0);
            }
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
        } else if (line.startsWith("CHANGE_PASSWORD_OK")) {
            emit changePasswordResult(true, "密码修改成功");
        } else if (line.startsWith("CHANGE_PASSWORD_FAIL")) {
            QString message = QString::fromUtf8(line.mid(20));
            emit changePasswordResult(false, message);
        } else if (line.startsWith("DELETE_USER_OK")) {
            emit deleteUserResult(true, "账号已注销");
        } else if (line.startsWith("DELETE_USER_FAIL")) {
            QString message = QString::fromUtf8(line.mid(16));
            emit deleteUserResult(false, message);
        } else if (line.startsWith("COPY_OK")) {
            emit copyResult(true, "复制成功");
        } else if (line.startsWith("COPY_FAIL")) {
            QString message = QString::fromUtf8(line.mid(9));
            emit copyResult(false, message);
        } else if (line.startsWith("MOVE_OK")) {
            emit moveResult(true, "移动成功");
        } else if (line.startsWith("MOVE_FAIL")) {
            QString message = QString::fromUtf8(line.mid(9));
            emit moveResult(false, message);
        } else if (line.startsWith("USER_INFO")) {
            QStringList parts = QString::fromUtf8(line.mid(10)).split(' ');
            if (parts.size() >= 5) {
                emit userInfoReceived(parts[0], parts[1], parts[2].toLongLong(), parts[3].toLongLong(), parts[4]);
            }
        } else if (line.startsWith("UPDATE_NICKNAME_OK")) {
            emit updateNicknameResult(true, "昵称修改成功");
        } else if (line.startsWith("UPDATE_NICKNAME_FAIL")) {
            emit updateNicknameResult(false, QString::fromUtf8(line.mid(20)));
        } else if (line.startsWith("UPDATE_EMAIL_OK")) {
            emit updateEmailResult(true, "邮箱修改成功");
        } else if (line.startsWith("UPDATE_EMAIL_FAIL")) {
            emit updateEmailResult(false, QString::fromUtf8(line.mid(17)));
        } else if (line.startsWith("UPDATE_AVATAR_OK")) {
            emit updateAvatarResult(true, "头像修改成功");
        } else if (line.startsWith("UPDATE_AVATAR_FAIL")) {
            emit updateAvatarResult(false, QString::fromUtf8(line.mid(18)));
        }
    }
}
