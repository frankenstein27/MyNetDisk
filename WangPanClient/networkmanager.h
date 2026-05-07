#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QByteArray>
#include <QThread>
#include <QFile>

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    static NetworkManager *instance();
    ~NetworkManager();

    bool connectToServer();
    void disconnectFromServer();
    bool isConnected() const;

    // 用户相关操作
    bool login(const QString &username, const QString &password);
    bool registerUser(const QString &username, const QString &email, const QString &password, const QString &nickname);

    // 文件相关操作
    bool uploadFile(const QString &localPath, const QString &remotePath);
    bool downloadFile(const QString &remotePath, const QString &localPath);
    bool listFiles(const QString &directory);

signals:
    void connected();
    void disconnected();
    void error(const QString &message);
    void loginResult(bool success, const QString &message);
    void registerResult(bool success, const QString &message);
    void fileListReceived(const QByteArray &data);
    void uploadResult(bool success, const QString &message);
    void downloadResult(bool success, const QString &message);
    void deleteResult(bool success, const QString &message);
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onReadyRead();

public:
    QTcpSocket *socket() const { return m_socket; }

private:
    NetworkManager(QObject *parent = nullptr);
    static NetworkManager *m_instance;

    QTcpSocket *m_socket;
    QByteArray m_buffer;
    QString m_serverAddress;
    int m_serverPort;
    bool m_isDownloading;
    
    QFile *m_downloadFile;
    QString m_downloadPath;
    qint64 m_downloadFileSize;
    qint64 m_downloadBytesReceived;
};

#endif // NETWORKMANAGER_H