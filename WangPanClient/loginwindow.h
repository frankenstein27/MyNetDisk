#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QString>

namespace Ui {
class LoginWindow;
}

class LoginWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit LoginWindow(QWidget* parent = nullptr);
    ~LoginWindow();

   signals:
    void loginSuccess(const QString& username);
    void registerRequested();

   private slots:
    void on_loginButton_clicked();
    void on_registerButton_clicked();
    void on_rememberPasswordCheckBox_toggled(bool checked);
    void on_autoLoginCheckBox_toggled(bool checked);

   private:
    void generateCaptcha();
    void loadSettings();
    void saveSettings();
    bool validateCaptcha();
    void performLogin(const QString& username, const QString& password);

   private:
    Ui::LoginWindow* ui;
    QSettings* m_settings;
    QString m_captcha;
};

#endif  // LOGINWINDOW_H