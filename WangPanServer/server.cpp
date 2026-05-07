#include "server.h"
#include "clienthandler.h"

Server::Server(QObject *parent) : QObject(parent)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &Server::onNewConnection);
}

Server::~Server()
{
    stop();
    delete m_server;
}

bool Server::start(quint16 port)
{
    if (!m_server->listen(QHostAddress::Any, port)) {
        qWarning("Failed to start server: %s", qPrintable(m_server->errorString()));
        return false;
    }
    qInfo("Server started on port %d", port);
    return true;
}

void Server::stop()
{
    m_server->close();
    for (ClientHandler *client : m_clients) {
        delete client;
    }
    m_clients.clear();
    qInfo("Server stopped");
}

void Server::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        ClientHandler *client = new ClientHandler(socket, this);
        m_clients.append(client);
        connect(client, &ClientHandler::disconnected, this, [=]() {
            m_clients.removeOne(client);
            delete client;
            emit clientDisconnected();
        });
        emit clientConnected();
        qInfo("New client connected: %s:%d", qPrintable(socket->peerAddress().toString()), socket->peerPort());
    }
}
