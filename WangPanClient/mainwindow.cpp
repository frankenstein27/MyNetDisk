#include "mainwindow.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileIconProvider>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QtGlobal>  // 确保 QT_VERSION 宏可用

#include "filemanager.h"
#include "networkmanager.h"
#include "ui_mainwindow.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QRegularExpression>
#else
#include <QRegExp>
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow), currentDirectory("/") {
    ui->setupUi(this);

    // 初始化目录树
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(ui->directoryTree);
    rootItem->setText(0, "我的网盘");
    rootItem->setData(0, Qt::UserRole, "/");
    ui->directoryTree->addTopLevelItem(rootItem);
    ui->directoryTree->expandAll();

    // 连接信号和槽
    connect(ui->directoryTree, &QTreeWidget::itemClicked, this, &MainWindow::HandleDirectoryTree_itemClicked);
    connect(ui->fileListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::HandleFileListWidget_itemDoubleClicked);
    connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::HandleBackButton_clicked);

    // 连接文件管理器的信号
    connect(FileManager::instance(), &FileManager::fileListUpdated, this, [=]() {
        // 更新文件列表UI
        ui->fileListWidget->clear();
        QList<FileInfo> fileList = FileManager::instance()->getFileList();

        // 检查是否有待处理的目录
        if (!pendingDirectory.isEmpty()) {
            pathHistory.append(currentDirectory);
            currentDirectory = pendingDirectory;
            pendingDirectory.clear();

            if (fileList.isEmpty()) {
                ui->statusLabel->setText("该目录为空");
            } else {
                // 成功进入且不为空，清除“正在进入”的提示
                ui->statusLabel->setText("已进入目录" + currentDirectory.split("/").back());
            }
        }

        QFileIconProvider iconProvider;
        for (int i = 0; i < fileList.size(); ++i) {
            FileInfo fileInfo = fileList[i];
            QListWidgetItem* item = new QListWidgetItem(fileInfo.name(), ui->fileListWidget);

            // 设置图标
            if (fileInfo.type() == "dir") {
                item->setIcon(iconProvider.icon(QFileIconProvider::Folder));
            } else {
                item->setIcon(iconProvider.icon(QFileIconProvider::File));
            }

            // 存储文件信息
            QVariantMap data;
            data["name"] = fileInfo.name();
            data["size"] = fileInfo.size();
            data["type"] = fileInfo.type();
            data["path"] = fileInfo.path();
            data["modifyTime"] = fileInfo.modifyTime();
            item->setData(Qt::UserRole, data);
        }

        // 更新路径标签
        ui->pathLabel->setText("当前路径: " + currentDirectory);
    });

    connect(FileManager::instance(), &FileManager::downloadResult, this, [=](bool success, const QString& message) {
        ui->progressBar->setVisible(false);
        m_downloadTimer.invalidate();
        if (success) {
            ui->statusLabel->setText("下载成功");

            // 如果是预览文件，打开预览窗口（复用已有窗口避免重复创建）
            if (!currentPreviewFile.isEmpty()) {
                QString tempPath = QDir::tempPath() + "/" + currentPreviewFile;

                // 如果已有预览窗口且未关闭，直接复用
                if (m_previewWindow && m_previewWindow->isVisible()) {
                    m_previewWindow->setFile(tempPath, currentPreviewFile);
                    m_previewWindow->raise();
                    m_previewWindow->activateWindow();
                } else {
                    PreviewWindow* previewWindow = new PreviewWindow(this);
                    previewWindow->setAttribute(Qt::WA_DeleteOnClose);
                    previewWindow->setFile(tempPath, currentPreviewFile);
                    previewWindow->show();
                    previewWindow->raise();
                    m_previewWindow = previewWindow;
                }
                ui->statusLabel->setText("预览文件: " + currentPreviewFile);
                currentPreviewFile.clear();
            }
        } else {
            ui->statusLabel->setText("下载失败: " + message);
            currentPreviewFile.clear();
        }
    });

    // 连接文件管理器的上传/下载进度信号，绑定到进度条并显示速度
    connect(FileManager::instance(), &FileManager::uploadProgress, this, [=](qint64 bytesSent, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            if (!m_uploadTimer.isValid()) m_uploadTimer.start();
            double elapsed = m_uploadTimer.elapsed() / 1000.0;
            double speed = elapsed > 0.01 ? bytesSent / elapsed : 0;
            QString speedStr;
            if (speed >= 1024 * 1024)
                speedStr = QString::number(speed / (1024 * 1024), 'f', 1) + " MB/s";
            else if (speed >= 1024)
                speedStr = QString::number(speed / 1024, 'f', 1) + " KB/s";
            else
                speedStr = QString::number(static_cast<int>(speed)) + " B/s";

            ui->progressBar->setVisible(true);
            ui->progressBar->setMaximum(100);
            int pct = static_cast<int>(bytesSent * 100 / bytesTotal);
            ui->progressBar->setValue(pct);
            ui->progressBar->setFormat(QString("上传 %1% (%2)").arg(pct).arg(speedStr));
        }
    });
    connect(FileManager::instance(), &FileManager::downloadProgress, this, [=](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            if (!m_downloadTimer.isValid()) m_downloadTimer.start();
            double elapsed = m_downloadTimer.elapsed() / 1000.0;
            double speed = elapsed > 0.01 ? bytesReceived / elapsed : 0;
            QString speedStr;
            if (speed >= 1024 * 1024)
                speedStr = QString::number(speed / (1024 * 1024), 'f', 1) + " MB/s";
            else if (speed >= 1024)
                speedStr = QString::number(speed / 1024, 'f', 1) + " KB/s";
            else
                speedStr = QString::number(static_cast<int>(speed)) + " B/s";

            ui->progressBar->setVisible(true);
            ui->progressBar->setMaximum(100);
            int pct = static_cast<int>(bytesReceived * 100 / bytesTotal);
            ui->progressBar->setValue(pct);
            ui->progressBar->setFormat(QString("下载 %1% (%2)").arg(pct).arg(speedStr));
        }
    });

    // 上传/删除成功后自动刷新当前目录并更新状态提示
    connect(FileManager::instance(), &FileManager::uploadResult, this, [=](bool success, const QString& message) {
        ui->progressBar->setVisible(false);
        m_uploadTimer.invalidate();
        if (success) {
            ui->statusLabel->setText("上传成功");
            NetworkManager::instance()->listFiles(currentDirectory);
        } else {
            ui->statusLabel->setText("上传失败: " + message);
        }
    });
    connect(FileManager::instance(), &FileManager::deleteResult, this, &MainWindow::onDeleteResult);

    // 初始化文件列表
    NetworkManager::instance()->listFiles(currentDirectory);
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::setUsername(const QString& username) {
    currentUsername = username;
    ui->userInfoLabel->setText("欢迎，" + username);
}

void MainWindow::updateDirectoryTree() {
    // 这里可以实现目录树的更新逻辑
}

QString MainWindow::buildPath(const QString& name) {
    if (currentDirectory == "/") {
        return "/" + name;
    } else {
        return currentDirectory + "/" + name;
    }
}

void MainWindow::on_actionNewDirectory_triggered() {
    bool ok;
    QString dirName = QInputDialog::getText(this, "新建目录", "请输入目录名称:", QLineEdit::Normal, "", &ok);
    if (ok && !dirName.isEmpty()) {
        // 校验系统不允许的文件名特殊字符
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QRegularExpression rx("[\\\\/:*?\"<>|]");
#else
        QRegExp rx("[\\\\/:*?\"<>|]");
#endif
        if (dirName.contains(rx)) {
            ui->statusLabel->setText("创建失败: 目录名不能包含 \\ / : * ? \" < > | 等字符");
            return;
        }

        QString path = buildPath(dirName);

        if (FileManager::instance()->createDirectory(path)) {
            ui->statusLabel->setText("目录 " + dirName + " 创建请求已发送");
            NetworkManager::instance()->listFiles(currentDirectory);
        } else {
            ui->statusLabel->setText("目录创建失败");
        }
    }
}

void MainWindow::on_actionUploadFile_triggered() {
    QString fileName = QFileDialog::getOpenFileName(this, "选择文件", QDir::homePath());
    if (!fileName.isEmpty()) {
        ui->statusLabel->setText("正在上传文件...");
        FileManager::instance()->uploadFile(fileName, buildPath(QFileInfo(fileName).fileName()));
    }
}

void MainWindow::on_actionDownloadFile_triggered() {
    QListWidgetItem* item = ui->fileListWidget->currentItem();
    if (item) {
        QVariantMap data = item->data(Qt::UserRole).toMap();
        QString fileName = data["name"].toString();
        QString savePath = QFileDialog::getSaveFileName(this, "保存文件", QDir::homePath() + "/" + fileName);
        if (!savePath.isEmpty()) {
            ui->statusLabel->setText("正在下载文件...");
            FileManager::instance()->downloadFile(buildPath(fileName), savePath);
        }
    } else {
        ui->statusLabel->setText("请先选择要下载的文件");
    }
}

void MainWindow::on_actionDelete_triggered() {
    QListWidgetItem* item = ui->fileListWidget->currentItem();
    if (item) {
        QVariantMap data = item->data(Qt::UserRole).toMap();
        QString fileName = data["name"].toString();
        QMessageBox::StandardButton reply = QMessageBox::question(this, "确认删除", "确定要删除文件 " + fileName + " 吗?", QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            FileManager::instance()->deleteFile(buildPath(fileName));
        }
    } else {
        ui->statusLabel->setText("请先选择要删除的文件");
    }
}

void MainWindow::on_actionRename_triggered() {
    QListWidgetItem* item = ui->fileListWidget->currentItem();
    if (item) {
        QVariantMap data = item->data(Qt::UserRole).toMap();
        QString oldFileName = data["name"].toString();
        bool ok;
        QString newFileName = QInputDialog::getText(this, "重命名文件", "请输入新的文件名:", QLineEdit::Normal, oldFileName, &ok);
        if (ok && !newFileName.isEmpty() && newFileName != oldFileName) {
            if (FileManager::instance()->renameFile(buildPath(oldFileName), buildPath(newFileName))) {
                ui->statusLabel->setText("文件重命名成功");
                NetworkManager::instance()->listFiles(currentDirectory);
            } else {
                ui->statusLabel->setText("文件重命名失败");
            }
        }
    } else {
        ui->statusLabel->setText("请先选择要重命名的文件");
    }
}

void MainWindow::on_actionExit_triggered() { QApplication::quit(); }

void MainWindow::on_uploadButton_clicked() {
    // 实现文件上传功能
    QString fileName = QFileDialog::getOpenFileName(this, "选择文件", QDir::homePath());
    if (!fileName.isEmpty()) {
        ui->statusLabel->setText("正在上传文件...");
        FileManager::instance()->uploadFile(fileName, buildPath(QFileInfo(fileName).fileName()));
    }
}

void MainWindow::on_downloadButton_clicked() {
    // 实现文件下载功能
    QListWidgetItem* item = ui->fileListWidget->currentItem();
    if (item) {
        QVariantMap data = item->data(Qt::UserRole).toMap();
        QString fileName = data["name"].toString();
        QString savePath = QFileDialog::getSaveFileName(this, "保存文件", QDir::homePath() + "/" + fileName);
        if (!savePath.isEmpty()) {
            ui->statusLabel->setText("正在下载文件...");
            FileManager::instance()->downloadFile(buildPath(fileName), savePath);
        }
    }
}

void MainWindow::on_logoutButton_clicked() { emit logoutRequested(); }

void MainWindow::HandleDirectoryTree_itemClicked(QTreeWidgetItem* item, int column) {
    currentDirectory = item->data(0, Qt::UserRole).toString();
    NetworkManager::instance()->listFiles(currentDirectory);
}

void MainWindow::HandleFileListWidget_itemDoubleClicked(QListWidgetItem* item) {
    // 获取文件信息
    QVariantMap data = item->data(Qt::UserRole).toMap();
    QString fileName = data["name"].toString();
    QString fileType = data["type"].toString();
    QString filePath = buildPath(fileName);

    // 如果是目录，进入目录
    if (fileType == "dir") {
        // 保存要进入的目录路径，等待文件列表返回后再更新
        pendingDirectory = filePath;

        // 刷新文件列表
        NetworkManager::instance()->listFiles(filePath);
        ui->statusLabel->setText("正在进入目录: " + fileName);
    } else {
        // 如果是文件，异步下载文件到临时目录进行预览
        currentPreviewFile = fileName;
        QString tempPath = QDir::tempPath() + "/" + fileName;

        if (QFile::exists(tempPath)) {
            QFile::remove(tempPath);
        }
        ui->statusLabel->setText("正在下载文件: " + fileName);
        FileManager::instance()->downloadFile(filePath, tempPath);
        // 下载结果由 downloadResult 信号异步通知，不在此处显示成功
    }
}

void MainWindow::HandleBackButton_clicked() {
    // 如果有历史路径，返回上一级目录
    if (!pathHistory.isEmpty()) {
        currentDirectory = pathHistory.takeLast();
        NetworkManager::instance()->listFiles(currentDirectory);
        ui->statusLabel->setText("返回目录: " + currentDirectory.split("/").back());
    } else {
        ui->statusLabel->setText("已经是根目录");
    }
}

void MainWindow::onDeleteResult(bool success, const QString& message) {
    if (success) {
        ui->statusLabel->setText("文件删除成功");
        // 刷新当前目录的文件列表
        NetworkManager::instance()->listFiles(currentDirectory);
    } else {
        ui->statusLabel->setText("文件删除失败: " + message);
    }
}

void MainWindow::on_actionRefresh_triggered() {
    ui->statusLabel->setText("正在刷新...");
    NetworkManager::instance()->listFiles(currentDirectory);
    ui->statusLabel->setText("刷新完成");
}

void MainWindow::on_actionChangePassword_triggered() {
    // 弹出修改密码对话框
    QDialog dlg(this);
    dlg.setWindowTitle("修改密码");
    QFormLayout form(&dlg);

    QLineEdit* oldPw = new QLineEdit(&dlg);
    oldPw->setEchoMode(QLineEdit::Password);
    oldPw->setPlaceholderText("请输入旧密码");
    QLineEdit* newPw = new QLineEdit(&dlg);
    newPw->setEchoMode(QLineEdit::Password);
    newPw->setPlaceholderText("请输入新密码");
    QLineEdit* confirmPw = new QLineEdit(&dlg);
    confirmPw->setEchoMode(QLineEdit::Password);
    confirmPw->setPlaceholderText("请确认新密码");

    form.addRow("旧密码:", oldPw);
    form.addRow("新密码:", newPw);
    form.addRow("确认密码:", confirmPw);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    QString old = oldPw->text(), nw = newPw->text(), cf = confirmPw->text();
    if (old.isEmpty() || nw.isEmpty() || cf.isEmpty()) {
        ui->statusLabel->setText("密码不能为空");
        return;
    }
    if (nw != cf) {
        ui->statusLabel->setText("两次输入的新密码不一致");
        return;
    }

    // 连接服务器响应（一次性）
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(NetworkManager::instance(), &NetworkManager::changePasswordResult, this, [=](bool success, const QString& msg) {
        ui->statusLabel->setText(success ? "密码修改成功" : "密码修改失败: " + msg);
        disconnect(*conn);
    });
    NetworkManager::instance()->changePassword(old, nw, cf);
}

void MainWindow::on_actionDeleteAccount_triggered() {
    auto reply = QMessageBox::warning(this, "注销账号", "确定要注销当前账号吗？\n此操作将删除您的所有文件和目录，且不可恢复！", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(NetworkManager::instance(), &NetworkManager::deleteUserResult, this, [=](bool success, const QString& msg) {
        disconnect(*conn);
        if (success) {
            QMessageBox::information(this, "注销成功", msg);
            emit logoutRequested();
        } else {
            QMessageBox::warning(this, "注销失败", msg);
        }
    });
    NetworkManager::instance()->deleteUser();
}

void MainWindow::on_actionClearPreview_triggered() {
    QDir tempDir = QDir::temp();
    QStringList filters;
    filters << "*.txt" << "*.cpp" << "*.h" << "*.c" << "*.hpp" << "*.java"
            << "*.py" << "*.js" << "*.html" << "*.css" << "*.sh"
            << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif" << "*.pdf";
    QFileInfoList files = tempDir.entryInfoList(filters, QDir::Files);
    if (files.isEmpty()) {
        ui->statusLabel->setText("没有需要清理的预览文件");
        return;
    }

    int count = 0;
    for (const QFileInfo& fi : files) {
        if (QFile::remove(fi.absoluteFilePath())) {
            ++count;
        }
    }
    ui->statusLabel->setText(QString("已清理 %1 个预览文件").arg(count));
}
