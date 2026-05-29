#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QString>

// 服务端文件管理（单例模式），负责文件的保存、读取、删除、重命名、移动等操作，提供用户空间管理和文件哈希计算等功能
class FileManager : public QObject {
    Q_OBJECT

   public:
    static FileManager* instance();
    ~FileManager();

    bool init(const QString& basePath);
    bool saveFile(const QString& username, const QString& filename, const QByteArray& data);
    QByteArray readFile(const QString& username, const QString& filename);
    bool deleteFile(const QString& username, const QString& filename);
    bool renameFile(const QString& username, const QString& oldFilename, const QString& newFilename);
    bool deleteDirectory(const QString& username, const QString& dirname);
    bool deleteUserFiles(const QString& username);
    bool moveFile(const QString& username, const QString& oldPath, const QString& newPath);
    qint64 getFileSize(const QString& username, const QString& filename);
    qint64 getUserUsedSpace(const QString& username);
    QString getFileHash(const QString& username, const QString& filename);

   private:
    FileManager(QObject* parent = nullptr);
    static FileManager* m_instance;

    QString m_basePath;
};

#endif  // FILEMANAGER_H