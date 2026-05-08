#include "clienthandler.h"

#include <QHostAddress>
#include <QUrl>

#include "databasemanager.h"
#include "filemanager.h"
#include "logmanager.h"
#include "usermanager.h"

// 协议字段百分号解码
static QString decodeField(const QString& field) { return QUrl::fromPercentEncoding(field.toUtf8()); }

ClientHandler::ClientHandler(QTcpSocket* socket, QObject* parent) : QObject(parent), m_socket(socket), m_loggedIn(false), m_uploading(false), m_uploadExpectedSize(0), m_uploadOffset(0) {
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);
}

ClientHandler::~ClientHandler() { m_socket->deleteLater(); }

void ClientHandler::onReadyRead() {
    // 读取所有可用数据
    QByteArray data = m_socket->readAll();
    m_buffer.append(data);

    // 如果正在接收上传数据，所有数据均为文件内容，不解析命令
    if (m_uploading) {
        continueUpload();
        return;
    }

    // 处理所有完整的请求行
    while (m_buffer.contains('\n')) {
        int pos = m_buffer.indexOf('\n');
        QByteArray lineData = m_buffer.left(pos);
        m_buffer.remove(0, pos + 1);

        QString line = QString::fromUtf8(lineData).trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(' ');
        if (parts.isEmpty()) continue;

        // 对所有协议字段进行百分号解码（还原空格等特殊字符）
        for (QString& part : parts) {
            part = decodeField(part);
        }

        QString command = parts[0];

        if (command == "LOGIN") {
            if (parts.size() >= 3) {
                QString username = parts[1];
                QString password = parts[2];
                handleLogin(username, password);
            }
        } else if (command == "REGISTER") {
            if (parts.size() >= 5) {
                QString username = parts[1];
                QString email = parts[2];
                QString password = parts[3];
                QString nickname = parts[4];
                handleRegister(username, email, password, nickname);
            }
        } else if (command == "LIST") {
            if (parts.size() >= 2) {
                QString directory = parts[1];
                handleListFiles(directory);
            }
        } else if (command == "UPLOAD_CHECK") {
            if (parts.size() >= 3) {
                QString remotePath = parts[1];
                QString fileName = parts[2];
                handleUploadCheck(remotePath, fileName);
            }
        } else if (command == "UPLOAD") {
            if (parts.size() >= 5) {
                QString remotePath = parts[1];
                qint64 fileSize = parts[2].toLongLong();
                QString fileName = parts[3];
                qint64 offset = parts[4].toLongLong();
                handleUpload(remotePath, fileSize, fileName, offset);
            }
        } else if (command == "DOWNLOAD") {
            if (parts.size() >= 3) {
                QString remotePath = parts[1];
                qint64 offset = parts[2].toLongLong();
                handleDownload(remotePath, offset);
            }
        } else if (command == "MKDIR") {
            if (parts.size() >= 2) {
                QString path = parts[1];  // 已由 decodeField 解码
                handleCreateDirectory(path);
            }
        } else if (command == "DELETE") {
            if (parts.size() >= 2) {
                QString path = parts[1];
                handleDelete(path);
            }
        } else if (command == "RENAME") {
            if (parts.size() >= 3) {
                QString oldPath = parts[1];
                QString newPath = parts[2];
                handleRename(oldPath, newPath);
            }
        } else if (command == "CHANGE_PASSWORD") {
            if (parts.size() >= 4) {
                QString oldPassword = parts[1];
                QString newPassword = parts[2];
                QString confirmPassword = parts[3];
                handleChangePassword(oldPassword, newPassword, confirmPassword);
            }
        } else if (command == "UPDATE_USER_INFO") {
            if (parts.size() >= 3) {
                QString email = parts[1];
                QString nickname = parts[2];
                handleUpdateUserInfo(email, nickname);
            }
        } else if (command == "DELETE_USER") {
            handleDeleteUser();
        }
    }
}

void ClientHandler::onDisconnected() {
    if (m_loggedIn) {
        LogManager::instance()->logUserAction(m_username, "logout", "user", m_username, "User logged out", m_socket->peerAddress().toString());
    }
    emit disconnected();
}

void ClientHandler::handleLogin(const QString& username, const QString& password) {
    if (UserManager::instance()->loginUser(username, password)) {
        m_username = username;
        m_loggedIn = true;
        DatabaseManager::instance()->updateLastLogin(username);
        LogManager::instance()->logUserAction(username, "login", "user", username, "User logged in", m_socket->peerAddress().toString());
        m_socket->write("LOGIN_OK\n");
    } else {
        m_socket->write("LOGIN_FAIL Invalid username or password\n");
    }
}

void ClientHandler::handleRegister(const QString& username, const QString& email, const QString& password, const QString& nickname) {
    if (UserManager::instance()->registerUser(username, email, password, nickname)) {
        LogManager::instance()->logUserAction(username, "register", "user", username, "User registered", m_socket->peerAddress().toString());
        m_socket->write("REGISTER_OK\n");
    } else {
        m_socket->write("REGISTER_FAIL Registration failed\n");
    }
}

void ClientHandler::handleListFiles(const QString& directory) {
    if (!m_loggedIn) {
        m_socket->write("LIST_FAIL Not logged in\n");
        return;
    }

    // 实现文件列表获取逻辑
    QList<QMap<QString, QVariant>> fileList;
    DatabaseManager::instance()->getFileList(m_username, directory, fileList);

    QString response = "FILE_LIST ";
    for (const QMap<QString, QVariant>& file : fileList) {
        response +=
            file["name"].toString() + "|" + QString::number(file["size"].toLongLong()) + "|" + file["type"].toString() + "|" + file["path"].toString() + "|" + file["last_modified"].toString() + "\t";
    }
    response += "\n";  // 确保响应以换行符结尾

    m_socket->write(response.toUtf8());
    m_socket->flush();
}

void ClientHandler::handleUploadCheck(const QString& remotePath, const QString& fileName) {
    if (!m_loggedIn) {
        m_socket->write("UPLOAD_CHECK_FAIL Not logged in\n");
        return;
    }

    // 检查文件是否存在及大小，使用完整路径
    qint64 fileSize = FileManager::instance()->getFileSize(m_username, remotePath);
    if (fileSize > 0) {
        m_socket->write("UPLOAD_RESUME " + QString::number(fileSize).toUtf8() + "\n");
    } else {
        m_socket->write("UPLOAD_NEW\n");
    }
}

void ClientHandler::handleUpload(const QString& remotePath, qint64 fileSize, const QString& fileName, qint64 offset) {
    qDebug() << "=== handleUpload (async) called ===";
    qDebug() << "remotePath:" << remotePath << "fileSize:" << fileSize << "fileName:" << fileName << "offset:" << offset;
    qDebug() << "m_username:" << m_username;

    if (!m_loggedIn) {
        m_socket->write("UPLOAD_FAIL Not logged in\n");
        return;
    }

    // 初始化异步上传状态
    m_uploading = true;
    m_uploadExpectedSize = fileSize - offset;
    m_uploadRemotePath = remotePath;
    m_uploadFileName = fileName;
    m_uploadOffset = offset;
    m_uploadData.clear();

    qDebug() << "Expected data size:" << m_uploadExpectedSize << "Bytes in m_buffer:" << m_buffer.size();

    // 立即消费 m_buffer 中已有的数据并继续接收
    continueUpload();
}

void ClientHandler::continueUpload() {
    if (!m_uploading) return;

    qint64 remaining = m_uploadExpectedSize - m_uploadData.size();

    // 从 m_buffer 中获取尽可能多的数据
    if (!m_buffer.isEmpty() && remaining > 0) {
        qint64 fromBuffer = qMin((qint64)m_buffer.size(), remaining);
        m_uploadData.append(m_buffer.left(fromBuffer));
        m_buffer.remove(0, fromBuffer);
        remaining -= fromBuffer;
        qDebug() << "Consumed" << fromBuffer << "bytes from buffer, total:" << m_uploadData.size() << "/" << m_uploadExpectedSize;
    }

    // 如果还没有收完，等待下一次 onReadyRead
    if (m_uploadData.size() < m_uploadExpectedSize) {
        qDebug() << "Waiting for more data (currently" << m_uploadData.size() << "of" << m_uploadExpectedSize << ")";
        return;
    }

    // 全部数据已收到，执行保存逻辑
    qDebug() << "Upload data complete:" << m_uploadData.size() << "bytes";
    m_uploading = false;

    QByteArray fileData = m_uploadData;

    // 处理断点续传：拼接已有文件
    if (m_uploadOffset > 0) {
        qDebug() << "Resuming upload: offset" << m_uploadOffset;
        QByteArray existingData = FileManager::instance()->readFile(m_username, m_uploadRemotePath);
        if (existingData.size() == m_uploadOffset) {
            fileData.prepend(existingData);
            qDebug() << "Combined file size:" << fileData.size();
        } else {
            qDebug() << "Existing file size mismatch, using new data only";
        }
    }

    // 保存文件并响应客户端
    if (FileManager::instance()->saveFile(m_username, m_uploadRemotePath, fileData)) {
        m_socket->write("UPLOAD_OK\n");
        m_socket->flush();
        qDebug() << "UPLOAD_OK sent";
        LogManager::instance()->logUserAction(m_username, "upload", "file", m_uploadFileName, "File uploaded", m_socket->peerAddress().toString());

        // 同步写入数据库 files 表
        int rootDirId = DatabaseManager::instance()->getRootDirectoryId(m_username);
        if (rootDirId >= 0) {
            QString ext = m_uploadFileName.contains('.') ? m_uploadFileName.section('.', -1) : "";
            DatabaseManager::instance()->addFile(m_username, m_uploadFileName, m_uploadRemotePath, fileData.size(), ext, rootDirId);

            // 写入数据库操作日志
            DatabaseManager::instance()->logAction(m_username, "upload", "file", m_uploadFileName, "上传文件: " + m_uploadRemotePath, m_socket->peerAddress().toString());
        }
    } else {
        m_socket->write("UPLOAD_FAIL Save failed\n");
        m_socket->flush();
        qDebug() << "UPLOAD_FAIL sent";
    }

    m_uploadData.clear();
}

void ClientHandler::handleDownload(const QString& remotePath, qint64 offset) {
    if (!m_loggedIn) {
        m_socket->write("DOWNLOAD_FAIL Not logged in\n");
        return;
    }

    // 读取文件，使用完整路径
    QByteArray fileData = FileManager::instance()->readFile(m_username, remotePath);
    if (fileData.isEmpty()) {
        m_socket->write("DOWNLOAD_FAIL File not found\n");
        return;
    }

    // 检查偏移量
    if (offset >= fileData.size()) {
        m_socket->write("DOWNLOAD_FAIL Invalid offset\n");
        return;
    }

    // 截取文件数据
    QByteArray partialData = fileData.mid(offset);

    // 发送文件大小和文件数据（异步写入，不阻塞事件循环）
    QString response = "DOWNLOAD_OK " + QString::number(fileData.size()) + "\n";
    m_socket->write(response.toUtf8());
    m_socket->write(partialData);
    m_socket->flush();

    QString fileName = remotePath.split('/').last();
    LogManager::instance()->logUserAction(m_username, "download", "file", fileName, "File downloaded", m_socket->peerAddress().toString());
    DatabaseManager::instance()->logAction(m_username, "download", "file", fileName, "下载文件: " + remotePath, m_socket->peerAddress().toString());
}

void ClientHandler::handleCreateDirectory(const QString& path) {
    if (!m_loggedIn) {
        m_socket->write("MKDIR_FAIL Not logged in\n");
        return;
    }

    // 实现创建目录逻辑
    if (DatabaseManager::instance()->createDirectory(m_username, path)) {
        m_socket->write("MKDIR_OK\n");
        QString directoryName = path.split('/').last();
        LogManager::instance()->logUserAction(m_username, "mkdir", "directory", directoryName, "Directory created", m_socket->peerAddress().toString());
        DatabaseManager::instance()->logAction(m_username, "mkdir", "directory", directoryName, "创建目录: " + path, m_socket->peerAddress().toString());
    } else {
        m_socket->write("MKDIR_FAIL Create failed\n");
    }
}

void ClientHandler::handleDelete(const QString& path) {
    if (!m_loggedIn) {
        m_socket->write("DELETE_FAIL Not logged in\n");
        return;
    }

    // 实现删除逻辑，使用完整路径
    if (FileManager::instance()->deleteFile(m_username, path)) {
        m_socket->write("DELETE_OK\n");
        QString fileName = path.split('/').last();
        LogManager::instance()->logUserAction(m_username, "delete", "file", fileName, "File deleted", m_socket->peerAddress().toString());
    } else {
        m_socket->write("DELETE_FAIL Delete failed\n");
    }
}

void ClientHandler::handleRename(const QString& oldPath, const QString& newPath) {
    if (!m_loggedIn) {
        m_socket->write("RENAME_FAIL Not logged in\n");
        return;
    }

    // 实现重命名逻辑，使用完整路径
    if (FileManager::instance()->renameFile(m_username, oldPath, newPath)) {
        m_socket->write("RENAME_OK\n");
        QString oldFileName = oldPath.split('/').last();
        QString newFileName = newPath.split('/').last();
        LogManager::instance()->logUserAction(m_username, "rename", "file", oldFileName, "File renamed to " + newFileName, m_socket->peerAddress().toString());
    } else {
        m_socket->write("RENAME_FAIL Rename failed\n");
    }
}

void ClientHandler::handleChangePassword(const QString& oldPassword, const QString& newPassword, const QString& confirmPassword) {
    if (!m_loggedIn) {
        m_socket->write("CHANGE_PASSWORD_FAIL Not logged in\n");
        return;
    }

    // 验证新密码和确认密码是否一致
    if (newPassword != confirmPassword) {
        m_socket->write("CHANGE_PASSWORD_FAIL Passwords do not match\n");
        return;
    }

    // 修改密码
    if (UserManager::instance()->changePassword(m_username, oldPassword, newPassword)) {
        m_socket->write("CHANGE_PASSWORD_OK\n");
        LogManager::instance()->logUserAction(m_username, "change_password", "user", m_username, "Password changed", m_socket->peerAddress().toString());
    } else {
        m_socket->write("CHANGE_PASSWORD_FAIL Old password is incorrect\n");
    }
}

void ClientHandler::handleUpdateUserInfo(const QString& email, const QString& nickname) {
    if (!m_loggedIn) {
        m_socket->write("UPDATE_USER_INFO_FAIL Not logged in\n");
        return;
    }

    // 更新用户信息
    if (UserManager::instance()->updateUserInfo(m_username, email, nickname)) {
        m_socket->write("UPDATE_USER_INFO_OK\n");
        LogManager::instance()->logUserAction(m_username, "update_info", "user", m_username, "User info updated", m_socket->peerAddress().toString());
    } else {
        m_socket->write("UPDATE_USER_INFO_FAIL Update failed\n");
    }
}

void ClientHandler::handleDeleteUser() {
    if (!m_loggedIn) {
        m_socket->write("DELETE_USER_FAIL Not logged in\n");
        return;
    }

    // 删除用户
    if (UserManager::instance()->deleteUser(m_username)) {
        m_socket->write("DELETE_USER_OK\n");
        LogManager::instance()->logUserAction(m_username, "delete_user", "user", m_username, "User deleted", m_socket->peerAddress().toString());
        m_loggedIn = false;
        m_username.clear();
    } else {
        m_socket->write("DELETE_USER_FAIL Delete failed\n");
    }
}
