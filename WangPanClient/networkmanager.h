#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QTcpSocket>
#include <QThread>

// 客户端网络管理类（核心，单例模式）TCP管理，协议发送接收，异步上传、下载状态机
class NetworkManager : public QObject {
    Q_OBJECT

   public:
    static NetworkManager* instance();
    ~NetworkManager();

    bool connectToServer();
    void disconnectFromServer();
    bool isConnected() const;

    // 用户相关操作
    void login(const QString& username, const QString& password);
    void registerUser(const QString& username, const QString& email, const QString& password, const QString& nickname);
    void changePassword(const QString& oldPassword, const QString& newPassword, const QString& confirmPassword);
    void deleteUser();

    // 文件、目录相关操作
    bool uploadFile(const QString& localPath, const QString& remotePath);
    bool downloadFile(const QString& remotePath, const QString& localPath);
    bool listFiles(const QString& directory);
    void copyFile(const QString& sourcePath, const QString& targetPath);
    void moveFile(const QString& sourcePath, const QString& targetPath);
    void getUserInfo();
    void updateNickname(const QString& nickname);
    void updateEmail(const QString& email);
    void updateAvatar(const QByteArray& imageData);

   signals:
    void connected();
    void disconnected();
    void error(const QString& message);
    void loginResult(bool success, const QString& message);
    void registerResult(bool success, const QString& message);
    void fileListReceived(const QByteArray& data);
    void uploadResult(bool success, const QString& message);
    void downloadResult(bool success, const QString& message);
    void deleteResult(bool success, const QString& message);
    void changePasswordResult(bool success, const QString& message);
    void deleteUserResult(bool success, const QString& message);
    void copyResult(bool success, const QString& message);
    void moveResult(bool success, const QString& message);
    void userInfoReceived(const QString& nickname, const QString& email, qint64 quota, qint64 usedSpace, const QString& avatar);
    void updateNicknameResult(bool success, const QString& message);
    void updateEmailResult(bool success, const QString& message);
    void updateAvatarResult(bool success, const QString& message);
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

   private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onReadyRead();
    void sendNextChunk();

   public:
    QTcpSocket* socket() const { return m_socket; }

   private:
    NetworkManager(QObject* parent = nullptr);
    static NetworkManager* m_instance;

    QTcpSocket* m_socket;
    QByteArray m_buffer;
    QString m_serverAddress;
    int m_serverPort;
    bool m_isDownloading;
    bool m_isUploading;

    QFile* m_downloadFile;
    QString m_downloadPath;
    qint64 m_downloadFileSize;
    qint64 m_downloadBytesReceived;

    // 异步上传状态
    QFile* m_uploadFile;
    qint64 m_uploadFileSize;
    qint64 m_uploadBytesSent;
    QString m_uploadRemotePath;

    // 断点续传：UPLOAD_CHECK 状态
    bool m_isCheckingUpload;
    QString m_checkLocalPath;
    QString m_checkRemotePath;
    QString m_checkFileName;

    void startUploadTransfer(qint64 offset);  // 发送 UPLOAD 头并开始传数据
};

#endif  // NETWORKMANAGER_H