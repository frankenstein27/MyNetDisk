#include "clienthandler.h"
#include "usermanager.h"
#include "databasemanager.h"
#include "logmanager.h"
#include <QHostAddress>
#include "filemanager.h"

ClientHandler::ClientHandler(QTcpSocket *socket, QObject *parent) : QObject(parent),
    m_socket(socket), m_loggedIn(false)
{
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);
}

ClientHandler::~ClientHandler()
{
    m_socket->deleteLater();
}

void ClientHandler::onReadyRead()
{
    // 读取所有可用数据
    QByteArray data = m_socket->readAll();
    m_buffer.append(data);

    // 处理所有完整的请求行
    while (m_buffer.contains('\n')) {
        int pos = m_buffer.indexOf('\n');
        QByteArray lineData = m_buffer.left(pos);
        m_buffer.remove(0, pos + 1);
        
        QString line = QString::fromUtf8(lineData).trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(' ');
        if (parts.isEmpty()) continue;

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
                QString path = parts[1];
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

void ClientHandler::onDisconnected()
{
    if (m_loggedIn) {
        LogManager::instance()->logUserAction(m_username, "logout", "user", m_username, "User logged out", m_socket->peerAddress().toString());
    }
    emit disconnected();
}

void ClientHandler::handleLogin(const QString &username, const QString &password)
{
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

void ClientHandler::handleRegister(const QString &username, const QString &email, const QString &password, const QString &nickname)
{
    if (UserManager::instance()->registerUser(username, email, password, nickname)) {
        LogManager::instance()->logUserAction(username, "register", "user", username, "User registered", m_socket->peerAddress().toString());
        m_socket->write("REGISTER_OK\n");
    } else {
        m_socket->write("REGISTER_FAIL Registration failed\n");
    }
}

void ClientHandler::handleListFiles(const QString &directory)
{
    if (!m_loggedIn) {
        m_socket->write("LIST_FAIL Not logged in\n");
        return;
    }

    // 实现文件列表获取逻辑
    QList<QMap<QString, QVariant>> fileList;
    DatabaseManager::instance()->getFileList(m_username, directory, fileList);

    QString response = "FILE_LIST ";
    for (const QMap<QString, QVariant> &file : fileList) {
        response += file["name"].toString() + "|" + 
                   QString::number(file["size"].toLongLong()) + "|" + 
                   file["type"].toString() + "|" + 
                   file["path"].toString() + "|" + 
                   file["last_modified"].toString() + "\t";
    }
    response += "\n";  // 确保响应以换行符结尾

    m_socket->write(response.toUtf8());
    m_socket->flush();
}

void ClientHandler::handleUploadCheck(const QString &remotePath, const QString &fileName)
{
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

void ClientHandler::handleUpload(const QString &remotePath, qint64 fileSize, const QString &fileName, qint64 offset)
{
    qDebug() << "=== handleUpload called ===";
    qDebug() << "remotePath:" << remotePath;
    qDebug() << "fileSize:" << fileSize;
    qDebug() << "fileName:" << fileName;
    qDebug() << "offset:" << offset;
    qDebug() << "m_username:" << m_username;
    
    if (!m_loggedIn) {
        m_socket->write("UPLOAD_FAIL Not logged in\n");
        return;
    }

    // 接收文件数据
    QByteArray fileData;
    qint64 bytesReceived = 0;
    qint64 expectedSize = fileSize - offset;

    // 添加调试信息
    qDebug() << "handleUpload: received request for file" << fileName;
    qDebug() << "File size:" << fileSize << "Offset:" << offset;
    qDebug() << "Expected data size:" << expectedSize;
    qDebug() << "Socket state:" << m_socket->state();
    qDebug() << "Bytes available:" << m_socket->bytesAvailable();

    // 改进的接收逻辑
    while (bytesReceived < expectedSize) {
        if (m_socket->bytesAvailable() > 0) {
            QByteArray data = m_socket->readAll();
            fileData.append(data);
            bytesReceived += data.size();
            qDebug() << "Received:" << bytesReceived << "/" << expectedSize;
        } else {
            // 等待数据，设置合理的超时时间
            qDebug() << "Waiting for data...";
            if (!m_socket->waitForReadyRead(10000)) {
                qDebug() << "Upload timeout: received" << bytesReceived << "expected" << expectedSize;
                m_socket->write("UPLOAD_FAIL Timeout\n");
                return;
            }
        }
    }

    qDebug() << "Upload complete: received" << bytesReceived << "bytes";

    // 验证接收的字节数
    if (bytesReceived != expectedSize) {
        qDebug() << "Upload failed: received" << bytesReceived << "but expected" << expectedSize;
        m_socket->write("UPLOAD_FAIL Invalid data size\n");
        return;
    }

    // 保存文件，使用完整路径
    qDebug() << "Saving file:" << remotePath << "with size:" << fileData.size();
    
    if (offset > 0) {
        // 续传模式，先读取现有文件
        qDebug() << "Resuming upload: offset" << offset;
        QByteArray existingData = FileManager::instance()->readFile(m_username, remotePath);
        qDebug() << "Existing file size:" << existingData.size();
        
        if (!existingData.isEmpty()) {
            // 确保现有数据大小与偏移量一致
            if (existingData.size() == offset) {
                existingData.append(fileData);
                fileData = existingData;
                qDebug() << "Combined file size:" << fileData.size();
            } else {
                qDebug() << "Existing file size doesn't match offset, starting from scratch";
            }
        }
    }

    if (FileManager::instance()->saveFile(m_username, remotePath, fileData)) {
        m_socket->write("UPLOAD_OK\n");
        m_socket->flush();
        qDebug() << "UPLOAD_OK sent";
        LogManager::instance()->logUserAction(m_username, "upload", "file", fileName, "File uploaded", m_socket->peerAddress().toString());
    } else {
        m_socket->write("UPLOAD_FAIL Save failed\n");
        m_socket->flush();
        qDebug() << "UPLOAD_FAIL sent";
    }
}

void ClientHandler::handleDownload(const QString &remotePath, qint64 offset)
{
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

    // 发送文件大小
    QString response = "DOWNLOAD_OK " + QString::number(fileData.size()) + "\n";
    m_socket->write(response.toUtf8());
    if (!m_socket->waitForBytesWritten()) {
        return;
    }

    // 发送文件数据
    m_socket->write(partialData);
    if (!m_socket->waitForBytesWritten()) {
        return;
    }

    QString fileName = remotePath.split('/').last();
    LogManager::instance()->logUserAction(m_username, "download", "file", fileName, "File downloaded", m_socket->peerAddress().toString());
}

void ClientHandler::handleCreateDirectory(const QString &path)
{
    if (!m_loggedIn) {
        m_socket->write("MKDIR_FAIL Not logged in\n");
        return;
    }

    // 实现创建目录逻辑
    if (DatabaseManager::instance()->createDirectory(m_username, path)) {
        m_socket->write("MKDIR_OK\n");
        QString directoryName = path.split('/').last();
        LogManager::instance()->logUserAction(m_username, "mkdir", "directory", directoryName, "Directory created", m_socket->peerAddress().toString());
    } else {
        m_socket->write("MKDIR_FAIL Create failed\n");
    }
}

void ClientHandler::handleDelete(const QString &path)
{
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

void ClientHandler::handleRename(const QString &oldPath, const QString &newPath)
{
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

void ClientHandler::handleChangePassword(const QString &oldPassword, const QString &newPassword, const QString &confirmPassword)
{
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

void ClientHandler::handleUpdateUserInfo(const QString &email, const QString &nickname)
{
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

void ClientHandler::handleDeleteUser()
{
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
