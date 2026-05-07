#include "registerwindow.h"
#include "ui_registerwindow.h"
#include "networkmanager.h"

RegisterWindow::RegisterWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::RegisterWindow)
{
    ui->setupUi(this);

    // 连接网络管理器的信号
    connect(NetworkManager::instance(), &NetworkManager::registerResult, this, [=](bool success, const QString &message) {
        if (success) {
            ui->statusLabel->setText("注册成功");
            emit registerSuccess();
            this->hide();
        } else {
            ui->statusLabel->setText("注册失败: " + message);
        }
    });
}

RegisterWindow::~RegisterWindow()
{
    delete ui;
}

void RegisterWindow::on_registerButton_clicked()
{
    QString username = ui->usernameLineEdit->text();
    QString email = ui->emailLineEdit->text();
    QString password = ui->passwordLineEdit->text();
    QString confirmPassword = ui->confirmPasswordLineEdit->text();
    QString nickname = ui->nicknameLineEdit->text();

    if (username.isEmpty() || email.isEmpty() || password.isEmpty() || confirmPassword.isEmpty() || nickname.isEmpty()) {
        ui->statusLabel->setText("请填写所有字段");
        return;
    }

    if (password != confirmPassword) {
        ui->statusLabel->setText("两次输入的密码不一致");
        return;
    }

    ui->statusLabel->setText("正在注册...");
    NetworkManager::instance()->registerUser(username, email, password, nickname);
}

void RegisterWindow::on_cancelButton_clicked()
{
    emit cancelRequested();
    this->hide();
}