#include <QApplication>
#include "loginwindow.h"
#include "registerwindow.h"
#include "mainwindow.h"
#include "networkmanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 初始化网络管理器
    NetworkManager::instance();

    // 创建登录窗口
    LoginWindow loginWindow;
    RegisterWindow registerWindow;
    MainWindow mainWindow;

    // 连接信号和槽
    QObject::connect(&loginWindow, &LoginWindow::registerRequested, &registerWindow, &RegisterWindow::show);
    QObject::connect(&registerWindow, &RegisterWindow::registerSuccess, &loginWindow, &LoginWindow::show);
    QObject::connect(&registerWindow, &RegisterWindow::cancelRequested, &loginWindow, &LoginWindow::show);
    QObject::connect(&loginWindow, &LoginWindow::loginSuccess, [&](const QString &username) {
        mainWindow.setUsername(username);
        mainWindow.show();
        loginWindow.hide();
    });
    QObject::connect(&mainWindow, &MainWindow::logoutRequested, [&]() {
        mainWindow.hide();
        loginWindow.show();
    });

    // 显示登录窗口
    loginWindow.show();

    return a.exec();
}