#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>

class FileInfo
{
public:
    FileInfo() {}
    FileInfo(const QString &name, qint64 size, const QString &type, const QString &path, const QString &modifyTime)
        : m_name(name), m_size(size), m_type(type), m_path(path), m_modifyTime(modifyTime) {}

    QString name() const { return m_name; }
    qint64 size() const { return m_size; }
    QString type() const { return m_type; }
    QString path() const { return m_path; }
    QString modifyTime() const { return m_modifyTime; }

private:
    QString m_name;
    qint64 m_size;
    QString m_type;
    QString m_path;
    QString m_modifyTime;
};

class FileManager : public QObject
{
    Q_OBJECT

public:
    static FileManager *instance();
    ~FileManager();

    QList<FileInfo> getFileList(const QString &directory);
    QList<FileInfo> getFileList();
    bool uploadFile(const QString &localPath, const QString &remotePath);
    bool downloadFile(const QString &remotePath, const QString &localPath);
    bool createDirectory(const QString &path);
    bool deleteFile(const QString &path);
    bool renameFile(const QString &oldPath, const QString &newPath);

public slots:
    void handleUploadResult(bool success, const QString &message);
    void handleDownloadResult(bool success, const QString &message);
    void handleDeleteResult(bool success, const QString &message);

signals:
    void fileListUpdated();
    void uploadResult(bool success, const QString &message);
    void downloadResult(bool success, const QString &message);
    void deleteResult(bool success, const QString &message);
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    FileManager(QObject *parent = nullptr);
    static FileManager *m_instance;

    QList<FileInfo> m_fileList;
};

#endif // FILEMANAGER_H