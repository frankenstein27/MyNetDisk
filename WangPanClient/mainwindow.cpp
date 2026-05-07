#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "filemanager.h"
#include "networkmanager.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QFileIconProvider>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    currentDirectory("/")
{
    ui->setupUi(this);

    // 初始化目录树
    QTreeWidgetItem *rootItem = new QTreeWidgetItem(ui->directoryTree);
    rootItem->setText(0, "我的网盘");
    rootItem->setData(0, Qt::UserRole, "/");
    ui->directoryTree->addTopLevelItem(rootItem);
    ui->directoryTree->expandAll();

    // 连接信号和槽
    connect(ui->directoryTree, &QTreeWidget::itemClicked, this, &MainWindow::on_directoryTree_itemClicked);
    connect(ui->fileListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::on_fileListWidget_itemDoubleClicked);
    connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::on_backButton_clicked);
    
    // 连接文件管理器的信号
    connect(FileManager::instance(), &FileManager::uploadResult, this, [=](bool success, const QString &message) {
        if (success) {
            ui->statusLabel->setText("上传成功");
            // 上传成功后发送刷新文件列表请求
            NetworkManager::instance()->listFiles(currentDirectory);
        } else {
            ui->statusLabel->setText("上传失败: " + message);
        }
    });
    
    connect(FileManager::instance(), &FileManager::downloadResult, this, [=](bool success, const QString &message) {
        if (success) {
            ui->statusLabel->setText("下载成功");
            
            // 如果是预览文件，打开预览窗口
            if (!currentPreviewFile.isEmpty()) {
                QString tempPath = QDir::tempPath() + "/" + currentPreviewFile;
                PreviewWindow *previewWindow = new PreviewWindow(this);
                previewWindow->setFile(tempPath, currentPreviewFile);
                previewWindow->show();
                ui->statusLabel->setText("预览文件: " + currentPreviewFile);
                currentPreviewFile.clear();
            }
        } else {
            ui->statusLabel->setText("下载失败: " + message);
            currentPreviewFile.clear();
        }
    });
    
    connect(FileManager::instance(), &FileManager::deleteResult, this, &MainWindow::onDeleteResult);
    
    // 连接文件列表更新信号
    connect(FileManager::instance(), &FileManager::fileListUpdated, this, [=]() {
        // 更新文件列表UI
        ui->fileListWidget->clear();
        QList<FileInfo> fileList = FileManager::instance()->getFileList();
        
        // 检查是否有待处理的目录
        if (!pendingDirectory.isEmpty()) {
                    // 只要点击了，无论里面是空还是满，都必须进入该目录更新路径
                    pathHistory.append(currentDirectory);
                    currentDirectory = pendingDirectory;
                    pendingDirectory.clear();

                    if (fileList.isEmpty()) {
                        ui->statusLabel->setText("该目录为空");
                    }
                }
        
        QFileIconProvider iconProvider;
        for (int i = 0; i < fileList.size(); ++i) {
            FileInfo fileInfo = fileList[i];
            QListWidgetItem *item = new QListWidgetItem(fileInfo.name(), ui->fileListWidget);
            
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
    
    // 连接文件管理器的上传进度信号
    connect(FileManager::instance(), &FileManager::uploadProgress, this, [=](qint64 bytesSent, qint64 bytesTotal) {
        qDebug() << "Upload progress:" << bytesSent << "/" << bytesTotal;
    });

    // 初始化文件列表
    NetworkManager::instance()->listFiles(currentDirectory);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setUsername(const QString &username)
{
    currentUsername = username;
    ui->userInfoLabel->setText("欢迎，" + username);
}

void MainWindow::updateDirectoryTree()
{
    // 这里可以实现目录树的更新逻辑
}

QString MainWindow::buildPath(const QString &name)
{
    if (currentDirectory == "/") {
        return "/" + name;
    } else {
        return currentDirectory + "/" + name;
    }
}

void MainWindow::on_actionNewDirectory_triggered()
{
    bool ok;
    QString dirName = QInputDialog::getText(this, "新建目录", "请输入目录名称:", QLineEdit::Normal, "", &ok);
    if (ok && !dirName.isEmpty()) {
        QString path = buildPath(dirName);
        
        if (FileManager::instance()->createDirectory(path)) {
            ui->statusLabel->setText("目录创建成功");
            NetworkManager::instance()->listFiles(currentDirectory);
        } else {
            ui->statusLabel->setText("目录创建失败");
        }
    }
}

void MainWindow::on_actionUploadFile_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择文件", QDir::homePath());
    if (!fileName.isEmpty()) {
        ui->statusLabel->setText("正在上传文件...");
        FileManager::instance()->uploadFile(fileName, buildPath(QFileInfo(fileName).fileName()));
    }
}

void MainWindow::on_actionDownloadFile_triggered()
{
    QListWidgetItem *item = ui->fileListWidget->currentItem();
    if (item) {
        QVariantMap data = item->data(Qt::UserRole).toMap();
        QString fileName = data["name"].toString();
        QString savePath = QFileDialog::getSaveFileName(this, "保存文件", QDir::homePath() + "/" + fileName);
        if (!savePath.isEmpty()) {
            if (QFile::exists(savePath)) {
                QFile::remove(savePath); // 强制覆盖，避开续传Bug
            }
            ui->statusLabel->setText("正在下载文件...");
            FileManager::instance()->downloadFile(buildPath(fileName), savePath);
        }
    } else {
        ui->statusLabel->setText("请先选择要下载的文件");
    }
}

void MainWindow::on_actionDelete_triggered()
{
    QListWidgetItem *item = ui->fileListWidget->currentItem();
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

void MainWindow::on_actionRename_triggered()
{
    QListWidgetItem *item = ui->fileListWidget->currentItem();
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

void MainWindow::on_actionExit_triggered()
{
    QApplication::quit();
}

void MainWindow::on_uploadButton_clicked()
{
    // 实现文件上传功能
    QString fileName = QFileDialog::getOpenFileName(this, "选择文件", QDir::homePath());
    if (!fileName.isEmpty()) {
        ui->statusLabel->setText("正在上传文件...");
        FileManager::instance()->uploadFile(fileName, buildPath(QFileInfo(fileName).fileName()));
    }
}

void MainWindow::on_downloadButton_clicked()
{
    // 实现文件下载功能
    QListWidgetItem *item = ui->fileListWidget->currentItem();
    if (item) {
        QVariantMap data = item->data(Qt::UserRole).toMap();
        QString fileName = data["name"].toString();
        QString savePath = QFileDialog::getSaveFileName(this, "保存文件", QDir::homePath() + "/" + fileName);
        if (!savePath.isEmpty()) {
            if (QFile::exists(savePath)) {
                QFile::remove(savePath); // 强制覆盖，避开续传Bug
            }
            ui->statusLabel->setText("正在下载文件...");
            FileManager::instance()->downloadFile(buildPath(fileName), savePath);
        }
    }
}

void MainWindow::on_logoutButton_clicked()
{
    emit logoutRequested();
}

void MainWindow::on_directoryTree_itemClicked(QTreeWidgetItem *item, int column)
{
    currentDirectory = item->data(0, Qt::UserRole).toString();
    NetworkManager::instance()->listFiles(currentDirectory);
}

void MainWindow::on_fileListWidget_itemDoubleClicked(QListWidgetItem *item)
{
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

        // 如果缓存已存在则必须删除，避免触发错误的断点续传
        if (QFile::exists(tempPath)) {
            QFile::remove(tempPath);
        }
        ui->statusLabel->setText("正在下载文件: " + fileName);
        FileManager::instance()->downloadFile(filePath, tempPath);
    }
}

void MainWindow::on_backButton_clicked()
{
    if (currentDirectory == "/" || currentDirectory.isEmpty()) {
        ui->statusLabel->setText("已经是根目录");
        return;
    }

    // 将路径用 '/' 切割成数组，自动过滤掉多余的斜杠
    QStringList parts = currentDirectory.split('/', QString::SkipEmptyParts);
    if (parts.size() <= 1) {
        currentDirectory = "/";
    } else {
        // 移除当前所在目录，重新拼接成上一级路径
        parts.removeLast();
        currentDirectory = "/" + parts.join("/");
    }

    NetworkManager::instance()->listFiles(currentDirectory);
    ui->statusLabel->setText("返回目录: " + currentDirectory);
}

void MainWindow::onDeleteResult(bool success, const QString &message)
{
    if (success) {
        ui->statusLabel->setText("文件删除成功");
        // 刷新当前目录的文件列表
        NetworkManager::instance()->listFiles(currentDirectory);
    } else {
        ui->statusLabel->setText("文件删除失败: " + message);
    }
}

void MainWindow::on_actionRefresh_triggered()
{
    ui->statusLabel->setText("正在刷新...");
    NetworkManager::instance()->listFiles(currentDirectory);
}
