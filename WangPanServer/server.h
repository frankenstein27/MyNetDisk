#ifndef SERVER_H
#define SERVER_H

#include <QList>
#include <QObject>
#include <QTcpServer>

// 向前声明 ClientHandler 类，避免在头文件中包含过多依赖
class ClientHandler;

// QTcpServer封装，负责监听端口和接受新连接，创建 ClientHandler 实例处理每个客户端
class Server : public QObject {
    Q_OBJECT

   public:
    explicit Server(QObject* parent = nullptr);
    ~Server();

    bool start(quint16 port);
    void stop();

   signals:
    void clientConnected();
    void clientDisconnected();

   private slots:
    void onNewConnection();

   private:
    QTcpServer* m_server;
    QList<ClientHandler*> m_clients;
};

#endif  // SERVER_H