#include <QCoreApplication>
#include <QSettings>

#include "databasemanager.h"
#include "filemanager.h"
#include "monitor.h"
#include "server.h"

// 服务端主程序入口
int main(int argc, char* argv[]) {
    QCoreApplication a(argc, argv);

    // 从配置文件读取端口号（配置文件与可执行文件同目录）
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    // 初始化数据库
    DatabaseManager::instance()->connect();

    // 初始化文件管理器
    FileManager::instance()->init("./files");

    // 启动监控
    Monitor::instance()->start();

    // 创建服务器实例(TCP监听)
    Server server;
    int port = settings.value("Server/port", 8888).toInt();
    server.start(port);

    return a.exec();
}