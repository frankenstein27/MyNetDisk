#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QObject>
#include <QString>

class UserManager : public QObject
{
    Q_OBJECT

public:
    static UserManager *instance();
    ~UserManager();

    bool registerUser(const QString &username, const QString &email, const QString &password, const QString &nickname);
    bool loginUser(const QString &username, const QString &password);
    bool changePassword(const QString &username, const QString &oldPassword, const QString &newPassword);
    bool updateUserInfo(const QString &username, const QString &email, const QString &nickname);
    bool deleteUser(const QString &username);

private:
    UserManager(QObject *parent = nullptr);
    static UserManager *m_instance;

    QString generateSalt();
    QString hashPassword(const QString &password, const QString &salt);
};

#endif // USERMANAGER_H