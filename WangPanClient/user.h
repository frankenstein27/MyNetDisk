#ifndef USER_H
#define USER_H

#include <QString>

class User
{
public:
    User();
    User(const QString &username, const QString &email, const QString &nickname);

    QString username() const;
    void setUsername(const QString &username);

    QString email() const;
    void setEmail(const QString &email);

    QString nickname() const;
    void setNickname(const QString &nickname);

    qint64 quota() const;
    void setQuota(qint64 quota);

    qint64 usedSpace() const;
    void setUsedSpace(qint64 usedSpace);

    QString avatar() const;
    void setAvatar(const QString &avatar);

private:
    QString m_username;
    QString m_email;
    QString m_nickname;
    qint64 m_quota;
    qint64 m_usedSpace;
    QString m_avatar;
};

#endif // USER_H