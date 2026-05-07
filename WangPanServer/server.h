#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QList>

class ClientHandler;

class Server : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);
    ~Server();

    bool start(quint16 port);
    void stop();

signals:
    void clientConnected();
    void clientDisconnected();

private slots:
    void onNewConnection();

private:
    QTcpServer *m_server;
    QList<ClientHandler *> m_clients;
};

#endif // SERVER_H