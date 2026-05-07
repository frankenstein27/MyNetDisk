#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QFileDialog>
#include <QDir>
#include "previewwindow.h"
#include <QTreeWidgetItem>
#include <QTableWidgetItem>
#include <QListWidgetItem>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setUsername(const QString &username);

public slots:
    void updateDirectoryTree();

signals:
    void logoutRequested();

private slots:
    void on_uploadButton_clicked();
    void on_downloadButton_clicked();
    void on_logoutButton_clicked();
    void HandleDirectoryTree_itemClicked(QTreeWidgetItem *item, int column);
    void HandleFileListWidget_itemDoubleClicked(QListWidgetItem *item);
    void on_actionNewDirectory_triggered();
    void on_actionUploadFile_triggered();
    void on_actionDownloadFile_triggered();
    void on_actionDelete_triggered();
    void on_actionRename_triggered();
    void on_actionExit_triggered();
    void onDeleteResult(bool success, const QString &message);
    void HandleBackButton_clicked();
    void on_actionRefresh_triggered();

private:
    Ui::MainWindow *ui;
    QString currentUsername;
    QString currentDirectory;
    QStringList pathHistory;
    QString currentPreviewFile;
    QString pendingDirectory;
    
    QString buildPath(const QString &name);
};

#endif // MAINWINDOW_H
