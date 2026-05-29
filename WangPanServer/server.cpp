#include "server.h"

#include "clienthandler.h"
#include "monitor.h"

Server::Server(QObject* parent) : QObject(parent) {
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &Server::onNewConnection);
}

Server::~Server() {
    stop();
    delete m_server;
}

bool Server::start(quint16 port) {
    if (!m_server->listen(QHostAddress::Any, port)) {
        qWarning("Failed to start server: %s", qPrintable(m_server->errorString()));
        return false;
    }
    qInfo("Server started on port %d", port);
    return true;
}

void Server::stop() {
    m_server->close();
    // 断开所有客户端连接并清理资源
    for (ClientHandler* client : m_clients) {
        delete client;
    }
    m_clients.clear();
    Monitor::instance()->setConnectionCount(0);
    qInfo("Server stopped");
}

// 创建-->注册-->断开连接后自动清理
void Server::onNewConnection() {
    // 由于一次事件循环中可能积累多个连接，使用while确保处理所有连接
    while (m_server->hasPendingConnections()) {
        // 从QTcpServer获取下一个待处理连接的套接字描述符
        QTcpSocket* socket = m_server->nextPendingConnection();
        // 为每一个连接创建一个ClientHandler实例，负责后续的通信处理
        ClientHandler* client = new ClientHandler(socket, this);
        // 添加到已连接客户端列表，并连接断开信号以便清理资源
        m_clients.append(client);
        // 连接客户端断开的信号，当客户端断开时从列表中移除并删除ClientHandler实例
        connect(client, &ClientHandler::disconnected, this, [=]() {     // 使用[=]，拷贝捕获
            m_clients.removeOne(client);
            Monitor::instance()->setConnectionCount(m_clients.size());
            delete client;
            emit clientDisconnected();
        });
        // 刷新连接数监控显示，并发出新连接信号
        Monitor::instance()->setConnectionCount(m_clients.size());
        emit clientConnected();
        qInfo("New client connected: %s:%d", qPrintable(socket->peerAddress().toString()), socket->peerPort());
    }
}
