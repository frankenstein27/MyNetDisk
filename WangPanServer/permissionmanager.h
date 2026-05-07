#ifndef PERMISSIONMANAGER_H
#define PERMISSIONMANAGER_H

#include <QObject>
#include <QString>

class PermissionManager : public QObject
{
    Q_OBJECT

public:
    static PermissionManager *instance();
    ~PermissionManager();

    bool checkPermission(const QString &username, const QString &path, int permissionType);
    bool setPermission(const QString &username, const QString &path, int permissionType);
    bool removePermission(const QString &username, const QString &path);

private:
    PermissionManager(QObject *parent = nullptr);
    static PermissionManager *m_instance;
};

#endif // PERMISSIONMANAGER_H