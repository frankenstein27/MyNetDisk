#include "loginwindow.h"

#include <QMessageBox>
#include <QPainter>
#include <QRandomGenerator>
#include <QTimer>

#include "networkmanager.h"
#include "ui_loginwindow.h"

LoginWindow::LoginWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::LoginWindow), m_settings(new QSettings("WangPan", "WangPanClient", this)) {
    ui->setupUi(this);

    // 生成验证码
    generateCaptcha();

    // 加载设置
    loadSettings();

    // 连接网络管理器的登录结果信号
    connect(NetworkManager::instance(), &NetworkManager::loginResult, this, [=](bool success, const QString& message) {
        if (success) {
            ui->statusLabel->setText("登录成功");
            // 保存设置
            saveSettings();
            emit loginSuccess(ui->usernameLineEdit->text());
            // 恢复登录按钮状态，允许用户再次尝试登录
            ui->loginButton->setEnabled(true);
            ui->statusLabel->setText("");
        } else {
            ui->statusLabel->setText("登录失败: " + message);
            // 重新生成验证码
            generateCaptcha();
            ui->captchaLineEdit->clear();
            // 恢复登录按钮状态，允许用户再次尝试登录
            ui->loginButton->setEnabled(true);
        }
    });

    // 连接服务器
    NetworkManager::instance()->connectToServer();
}

LoginWindow::~LoginWindow() {
    delete m_settings;
    delete ui;
}

/// @brief 生成4位随机验证码
void LoginWindow::generateCaptcha() {
    // 不管当前状态，总之先清空验证码字符串
    m_captcha.clear();
    for (int i = 0; i < 4; ++i) {
        // 利用Qt的随机数生成器生成一个0-25之间的随机数，然后转换成对应的字母（A-Z），添加到验证码字符串尾部
        m_captcha.append(QChar('A' + QRandomGenerator::global()->bounded(26)));
    }

    // 在验证码标签上显示
    ui->captchaLabel->setText(m_captcha);
    ui->captchaLabel->setStyleSheet("QLabel { color: blue; font-size: 20px; font-weight: bold; background-color: lightgray; padding: 5px; }");
}

void LoginWindow::loadSettings() {
    // 加载记住密码和自动登录设置
    bool rememberPassword = m_settings->value("rememberPassword", false).toBool();
    bool autoLogin = m_settings->value("autoLogin", false).toBool();
    QString savedUsername = m_settings->value("username", "").toString();
    QString savedPassword = m_settings->value("password", "").toString();

    ui->rememberPasswordCheckBox->setChecked(rememberPassword);
    ui->autoLoginCheckBox->setChecked(autoLogin);
    ui->usernameLineEdit->setText(savedUsername);

    if (rememberPassword) {
        ui->passwordLineEdit->setText(savedPassword);
    }

    // 如果启用了自动登录，则自动触发登录
    if (autoLogin && !savedUsername.isEmpty() && !savedPassword.isEmpty()) {
        // 自动填充验证码输入框，方便自动登录
        ui->captchaLineEdit->setText(m_captcha);

        // 保存瞬间输入的用户名和密码，避免用户在定时器触发之前修改了输入框的内容导致登录失败
        QString temp_username = ui->usernameLineEdit->text();
        QString temp_password = ui->passwordLineEdit->text();

        // 单次定时器，1秒后调用performLogin函数
        QTimer::singleShot(1000, this, [this, temp_username, temp_password]() { performLogin(temp_username, temp_password); });
        // 禁用登录按钮，防止用户在自动登录过程中点击登录导致重复请求出现异常
        ui->loginButton->setEnabled(false);
    }
}

void LoginWindow::saveSettings() {
    if (ui->rememberPasswordCheckBox->isChecked()) {
        m_settings->setValue("rememberPassword", true);
        m_settings->setValue("username", ui->usernameLineEdit->text());
        m_settings->setValue("password", ui->passwordLineEdit->text());
    } else {
        m_settings->setValue("rememberPassword", false);
        m_settings->setValue("username", "");
        m_settings->setValue("password", "");
    }

    m_settings->setValue("autoLogin", ui->autoLoginCheckBox->isChecked());
}

bool LoginWindow::validateCaptcha() {
    QString input = ui->captchaLineEdit->text().toUpper();
    return input == m_captcha;
}

void LoginWindow::on_loginButton_clicked() {
    QString username = ui->usernameLineEdit->text();
    QString password = ui->passwordLineEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        ui->statusLabel->setText("用户名或密码不能为空");
        return;
    }

    // 验证验证码
    if (!validateCaptcha()) {
        ui->statusLabel->setText("验证码错误");
        generateCaptcha();
        ui->captchaLineEdit->clear();
        return;
    }

    performLogin(username, password);
}

void LoginWindow::performLogin(const QString& username, const QString& password) {
    ui->statusLabel->setText("正在登录...");
    NetworkManager::instance()->login(username, password);
}

void LoginWindow::on_registerButton_clicked() {
    emit registerRequested();
    // 隐藏自身（登陆界面）
    this->hide();
}

void LoginWindow::on_rememberPasswordCheckBox_toggled(bool checked) {
    // 如果取消记住密码，则自动取消自动登录
    if (!checked) {
        ui->autoLoginCheckBox->setChecked(false);
    }
}

void LoginWindow::on_autoLoginCheckBox_toggled(bool checked) {
    // 如果启用自动登录，则自动勾选记住密码
    if (checked) {
        ui->rememberPasswordCheckBox->setChecked(true);
    }
}