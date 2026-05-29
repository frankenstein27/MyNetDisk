#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QLabel>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QTableWidgetItem>
#include <QTreeWidgetItem>

#include "previewwindow.h"
#include "user.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void setUsername(const QString& username);

   signals:
    // 登出信号，通知主窗口切换回登录界面
    void logoutRequested();

   private slots:
    void on_uploadButton_clicked();
    void on_downloadButton_clicked();
    void on_logoutButton_clicked();
    void HandleFileListWidget_itemDoubleClicked(QListWidgetItem* item);

    void on_actionNewDirectory_triggered();
    void on_actionUploadFile_triggered();
    void on_actionDownloadFile_triggered();
    void on_actionDelete_triggered();
    void on_actionRename_triggered();
    void on_actionExit_triggered();
    void on_actionChangePassword_triggered();
    void on_actionDeleteAccount_triggered();
    void on_actionClearPreview_triggered();
    void on_actionCopy_triggered();
    void on_actionCut_triggered();
    void on_actionPaste_triggered();
    void on_actionUpdateAvatar_triggered();
    void on_actionUpdateNickname_triggered();
    void on_actionUpdateEmail_triggered();

    void onDeleteResult(bool success, const QString& message);
    void HandleBackButton_clicked();

   private:
    Ui::MainWindow* ui;
    QString currentUsername;
    QString currentDirectory;
    QStringList pathHistory;
    QString currentPreviewFile;
    QString pendingDirectory;
    QPointer<PreviewWindow> m_previewWindow;

    // 剪贴板状态
    QStringList m_clipboardPaths;  // 剪贴板中的路径列表
    QString m_clipboardAction;     // "copy" 或 "cut"
    QString m_clipboardSourceDir;  // 剪贴板操作发生的源目录

    // 用户信息
    User m_currentUser;

    // 传输速度计时
    QElapsedTimer m_uploadTimer;
    QElapsedTimer m_downloadTimer;

    QString buildPath(const QString& name);

    // UI 辅助
    QLabel* m_avatarLabel;
    QLabel* m_nicknameLabel;
    QLabel* m_storageLabel;
    void updateStorageDisplay();
    void updateUserDisplay();
    void refreshUserInfo();
    static bool isFileExtensionForbidden(const QString& filename);
    static const qint64 MAX_UPLOAD_SIZE;
};

#endif  // MAINWINDOW_H
