#include <QCoreApplication>
#include "server.h"
#include "monitor.h"
#include "databasemanager.h"
#include "filemanager.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // 初始化数据库
    DatabaseManager::instance()->connect();

    // 初始化文件管理器
    FileManager::instance()->init("./files");

    // 启动监控
    Monitor::instance()->start();

    // 创建服务器实例
    Server server;
    server.start(8888);

    return a.exec();
}