#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QTcpSocket>

class ClientHandler : public QObject {
    Q_OBJECT

   public:
    explicit ClientHandler(QTcpSocket* socket, QObject* parent = nullptr);
    ~ClientHandler();

   signals:
    void disconnected();

   private slots:
    void onReadyRead();
    void onDisconnected();

   private:
    void handleLogin(const QString& username, const QString& password);
    void handleRegister(const QString& username, const QString& email, const QString& password, const QString& nickname);
    void handleListFiles(const QString& directory);
    void handleUploadCheck(const QString& remotePath, const QString& fileName);
    void handleUpload(const QString& remotePath, qint64 fileSize, const QString& fileName, qint64 offset);
    void handleDownload(const QString& remotePath, qint64 offset);
    void continueUpload();  // 异步上传状态机继续接收数据
    void handleCreateDirectory(const QString& path);
    void handleDelete(const QString& path);
    void handleRename(const QString& oldPath, const QString& newPath);
    void handleChangePassword(const QString& oldPassword, const QString& newPassword, const QString& confirmPassword);
    void handleUpdateUserInfo(const QString& email, const QString& nickname);
    void handleDeleteUser();

   private:
    QTcpSocket* m_socket;
    QString m_username;
    bool m_loggedIn;
    QByteArray m_buffer;

    // 异步上传状态
    bool m_uploading;
    QByteArray m_uploadData;
    qint64 m_uploadExpectedSize;
    QString m_uploadRemotePath;
    QString m_uploadFileName;
    qint64 m_uploadOffset;
};

#endif  // CLIENTHANDLER_H