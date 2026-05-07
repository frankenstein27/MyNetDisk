#include "user.h"

User::User() : m_quota(10737418240), m_usedSpace(0)
{
}

User::User(const QString &username, const QString &email, const QString &nickname)
    : m_username(username), m_email(email), m_nickname(nickname), m_quota(10737418240), m_usedSpace(0)
{
}

QString User::username() const
{
    return m_username;
}

void User::setUsername(const QString &username)
{
    m_username = username;
}

QString User::email() const
{
    return m_email;
}

void User::setEmail(const QString &email)
{
    m_email = email;
}

QString User::nickname() const
{
    return m_nickname;
}

void User::setNickname(const QString &nickname)
{
    m_nickname = nickname;
}

qint64 User::quota() const
{
    return m_quota;
}

void User::setQuota(qint64 quota)
{
    m_quota = quota;
}

qint64 User::usedSpace() const
{
    return m_usedSpace;
}

void User::setUsedSpace(qint64 usedSpace)
{
    m_usedSpace = usedSpace;
}

QString User::avatar() const
{
    return m_avatar;
}

void User::setAvatar(const QString &avatar)
{
    m_avatar = avatar;
}
