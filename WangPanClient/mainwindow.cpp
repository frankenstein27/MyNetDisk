#include "mainwindow.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileIconProvider>
#include <QFormLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QVBoxLayout>
#include <QtGlobal>  // 确保 QT_VERSION 宏可用

#include "filemanager.h"
#include "networkmanager.h"
#include "ui_mainwindow.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QRegularExpression>
#else
#include <QRegExp>
#endif

// 客户端侧危险后缀黑名单校验（与后端保持一致）
static const QSet<QString> s_forbiddenExtensions = {"sh", "exe", "bat", "cmd", "msi", "apk", "dll", "scr", "com", "pif", "vbs", "ps1", "jar", "app", "run", "deb", "rpm"};

bool MainWindow::isFileExtensionForbidden(const QString& filename) {
    int dotPos = filename.lastIndexOf('.');
    if (dotPos < 0) return false;
    // 获取文件后缀
    QString ext = filename.mid(dotPos + 1).toLower();
    return s_forbiddenExtensions.contains(ext);
}

const qint64 MainWindow::MAX_UPLOAD_SIZE = 512LL * 1024 * 1024;  // 单个文件上传限制512MB

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow), currentDirectory("/") {
    ui->setupUi(this);

    // ===== 初始化指针 =====
    // 复用 UI 中已有的 userInfoLabel 作为昵称标签
    m_nicknameLabel = ui->userInfoLabel;
    m_nicknameLabel->setText("加载中...");

    // 头像标签：插入到顶部布局最前面
    m_avatarLabel = new QLabel(ui->topWidget);
    m_avatarLabel->setFixedSize(36, 36);
    m_avatarLabel->setScaledContents(true);
    m_avatarLabel->setStyleSheet("border-radius: 18px; border: 2px solid #ddd;");
    QPixmap defaultAvatar(36, 36);
    defaultAvatar.fill(QColor(200, 200, 200));
    {
        QPainter painter(&defaultAvatar);
        painter.setBrush(QColor(150, 150, 150));
        painter.drawEllipse(4, 4, 28, 28);
    }
    m_avatarLabel->setPixmap(defaultAvatar);

    // 存储空间标签
    m_storageLabel = new QLabel("存储: -- / --", ui->topWidget);
    m_storageLabel->setStyleSheet("font-size: 12px; color: #666;");

    // 将头像和存储标签插入布局
    QHBoxLayout* topLayout = qobject_cast<QHBoxLayout*>(ui->topWidget->layout());
    if (topLayout) {
        topLayout->insertWidget(0, m_avatarLabel);
        topLayout->insertWidget(2, m_storageLabel);
    }

    // 连接信号和槽
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

    // 连接用户信息接收信号，更新头像、昵称、存储空间
    connect(NetworkManager::instance(), &NetworkManager::userInfoReceived, this, [=](const QString& nickname, const QString& email, qint64 quota, qint64 usedSpace, const QString& avatar) {
        m_currentUser.setNickname(nickname);
        m_currentUser.setEmail(email);
        m_currentUser.setQuota(quota);
        m_currentUser.setUsedSpace(usedSpace);
        m_currentUser.setAvatar(avatar);
        updateUserDisplay();
        updateStorageDisplay();
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
            refreshUserInfo();  // 刷新存储空间显示
        } else {
            ui->statusLabel->setText("上传失败: " + message);
        }
    });
    connect(FileManager::instance(), &FileManager::deleteResult, this, &MainWindow::onDeleteResult);

    // 头像/昵称/邮箱修改成功后实时刷新用户信息
    connect(NetworkManager::instance(), &NetworkManager::updateAvatarResult, this, [=](bool success, const QString&) {
        if (success) refreshUserInfo();
    });
    connect(NetworkManager::instance(), &NetworkManager::updateNicknameResult, this, [=](bool success, const QString&) {
        if (success) refreshUserInfo();
    });
    connect(NetworkManager::instance(), &NetworkManager::updateEmailResult, this, [=](bool success, const QString&) {
        if (success) refreshUserInfo();
    });

    // 初始化文件列表
    NetworkManager::instance()->listFiles(currentDirectory);
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::setUsername(const QString& username) {
    currentUsername = username;
    m_nicknameLabel->setText(username);  // 临时显示，refreshUserInfo 后会更新为昵称

    // 登录成功后获取用户信息并刷新文件列表
    refreshUserInfo();
    NetworkManager::instance()->listFiles("/");
}

void MainWindow::updateUserDisplay() {
    m_nicknameLabel->setText(m_currentUser.nickname().isEmpty() ? currentUsername : m_currentUser.nickname());

    // 加载头像：从 Base64 解码显示
    QString avatarBase64 = m_currentUser.avatar();
    if (!avatarBase64.isEmpty()) {
        QByteArray avatarData = QByteArray::fromBase64(avatarBase64.toUtf8());
        QPixmap pix;
        if (pix.loadFromData(avatarData) && !pix.isNull()) {
            m_avatarLabel->setPixmap(pix.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
    }
    // 默认头像
    QPixmap defaultAvatar(36, 36);
    defaultAvatar.fill(QColor(200, 200, 200));
    QPainter painter(&defaultAvatar);
    painter.setBrush(QColor(150, 150, 150));
    painter.drawEllipse(4, 4, 28, 28);
    painter.end();
    m_avatarLabel->setPixmap(defaultAvatar);
}

void MainWindow::updateStorageDisplay() {
    qint64 used = m_currentUser.usedSpace();
    qint64 total = m_currentUser.quota();

    auto formatSize = [](qint64 bytes) -> QString {
        if (bytes >= 1024 * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024 * 1024), 'f', 2) + " GB";
        if (bytes >= 1024 * 1024) return QString::number(bytes / (1024.0 * 1024), 'f', 1) + " MB";
        if (bytes >= 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
        return QString::number(bytes) + " B";
    };

    m_storageLabel->setText("存储: " + formatSize(used) + " / " + formatSize(total));
}

void MainWindow::refreshUserInfo() { NetworkManager::instance()->getUserInfo(); }

QString MainWindow::buildPath(const QString& name) {
    if (currentDirectory == "/") {
        return "/" + name;
    } else {
        return currentDirectory + "/" + name;
    }
}

/// @brief 创建目录的处理函数，包含输入验证和同名冲突检测
void MainWindow::on_actionNewDirectory_triggered() {
    bool ok;
    QString dirName = QInputDialog::getText(this, "新建目录", "请输入目录名称:", QLineEdit::Normal, "", &ok);
    if (ok && !dirName.isEmpty()) {
        // 由于Windows和Linux安装的Qt版本不同，此处只能使用条件编译，正常情况下使用一种即可
        // 由于Windows和Linux的文件系统限制，需要进行校验，禁止创建包含特殊字符的目录
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

        // 检查同名冲突
        if (FileManager::instance()->nameExists(dirName)) {
            QMessageBox::StandardButton reply = QMessageBox::question(this, "同名冲突", QString("目录 \"%1\" 已存在，是否覆盖？").arg(dirName), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (reply != QMessageBox::Yes) {
                ui->statusLabel->setText("创建目录已取消");
                return;
            }
        }

        if (FileManager::instance()->createDirectory(path)) {
            ui->statusLabel->setText("目录 " + dirName + " 创建请求已发送");
            NetworkManager::instance()->listFiles(currentDirectory);
        } else {
            ui->statusLabel->setText("目录创建失败");
        }
    }
}

void MainWindow::on_actionUploadFile_triggered() {
    // 打开一个文件选择框，由用户选择要上传的文件
    QString fileName = QFileDialog::getOpenFileName(this, "选择文件", QDir::homePath());
    if (!fileName.isEmpty()) {
        QFileInfo fi(fileName);
        // 客户端安全校验：后缀名黑名单
        if (isFileExtensionForbidden(fi.fileName())) {
            QMessageBox::warning(this, "禁止上传", "不允许上传此类型的文件（" + fi.suffix() + "），可能为可执行文件或高风险文件。");
            return;
        }
        // 客户端安全校验：文件大小限制
        if (fi.size() > MAX_UPLOAD_SIZE) {
            QMessageBox::warning(this, "文件过大", "单个文件不能超过 512MB。");
            return;
        }

        // 同名冲突检测
        QString remotePath = buildPath(fi.fileName());
        if (FileManager::instance()->nameExists(fi.fileName())) {
            QMessageBox::StandardButton reply =
                QMessageBox::question(this, "同名冲突", QString("文件 \"%1\" 已存在，是否覆盖？").arg(fi.fileName()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (reply != QMessageBox::Yes) {
                ui->statusLabel->setText("上传已取消");
                return;
            }
        }

        ui->statusLabel->setText("正在上传文件...");
        FileManager::instance()->uploadFile(fileName, remotePath);
    }
}

void MainWindow::on_actionDownloadFile_triggered() {
    // 获取当前选中的文件项
    QListWidgetItem* item = ui->fileListWidget->currentItem();
    if (item) {  // 如果选中了
        QVariantMap data = item->data(Qt::UserRole).toMap();
        QString fileName = data["name"].toString();
        // 打开文件路径选择对话框，用户选择文件保存位置
        QString savePath = QFileDialog::getSaveFileName(this, "保存文件", QDir::homePath() + "/" + fileName);
        if (!savePath.isEmpty()) {
            ui->statusLabel->setText("正在下载文件...");
            FileManager::instance()->downloadFile(buildPath(fileName), savePath);
        }
    } else {  // 未选中任何内容
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
        if (isFileExtensionForbidden(newFileName)) {
            QString suffix = QFileInfo(newFileName).suffix();
            QMessageBox::warning(this, "禁止重命名", "不允许重命名为此类型的文件（" + suffix + "），可能为可执行文件或高风险文件。");
            return;
        }
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
        QFileInfo fi(fileName);
        if (isFileExtensionForbidden(fi.fileName())) {
            QMessageBox::warning(this, "禁止上传", "不允许上传此类型的文件（" + fi.suffix() + "），可能为可执行文件或高风险文件。");
            return;
        }
        if (fi.size() > MAX_UPLOAD_SIZE) {
            QMessageBox::warning(this, "文件过大", "单个文件不能超过 512MB。");
            return;
        }

        // 同名冲突检测
        QString remotePath = buildPath(fi.fileName());
        if (FileManager::instance()->nameExists(fi.fileName())) {
            QMessageBox::StandardButton reply =
                QMessageBox::question(this, "同名冲突", QString("文件 \"%1\" 已存在，是否覆盖？").arg(fi.fileName()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (reply != QMessageBox::Yes) {
                ui->statusLabel->setText("上传已取消");
                return;
            }
        }

        ui->statusLabel->setText("正在上传文件...");
        FileManager::instance()->uploadFile(fileName, remotePath);
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

/// @brief 双击文件列表项的处理函数，根据类型执行不同操作
/// @details 如果是目录，则进入目录；如果是文件，则下载到临时目录。
/// @param item 双击的文件项
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
        // 下载结果由 downloadResult 信号异步通知，异步返回后显示在statusLabel上
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
        NetworkManager::instance()->listFiles(currentDirectory);
        refreshUserInfo();
    } else {
        ui->statusLabel->setText("文件删除失败: " + message);
    }
}

// ===== 剪贴板功能 =====

void MainWindow::on_actionCopy_triggered() {
    // 获取选中的文件/目录项，保存到剪贴板路径列表，并记录是复制还是剪切
    QList<QListWidgetItem*> selected = ui->fileListWidget->selectedItems();
    if (selected.isEmpty()) {
        ui->statusLabel->setText("请先选择要复制的文件/文件夹");
        return;
    }
    m_clipboardPaths.clear();
    for (auto* item : selected) {
        QVariantMap data = item->data(Qt::UserRole).toMap();
        m_clipboardPaths.append(data["name"].toString());
    }
    m_clipboardAction = "copy";
    m_clipboardSourceDir = currentDirectory;
    ui->statusLabel->setText(QString("已复制 %1 个项目").arg(m_clipboardPaths.size()));
}

void MainWindow::on_actionCut_triggered() {
    QList<QListWidgetItem*> selected = ui->fileListWidget->selectedItems();
    if (selected.isEmpty()) {
        ui->statusLabel->setText("请先选择要剪切的文件/文件夹");
        return;
    }
    m_clipboardPaths.clear();
    for (auto* item : selected) {
        QVariantMap data = item->data(Qt::UserRole).toMap();
        m_clipboardPaths.append(data["name"].toString());
    }
    m_clipboardAction = "cut";
    m_clipboardSourceDir = currentDirectory;
    ui->statusLabel->setText(QString("已剪切 %1 个项目").arg(m_clipboardPaths.size()));
}

void MainWindow::on_actionPaste_triggered() {
    if (m_clipboardAction.isEmpty() || m_clipboardPaths.isEmpty()) {
        ui->statusLabel->setText("剪贴板为空，请先复制或剪切文件");
        return;
    }

    // 检查目标是否存在同名文件/目录
    bool hasConflict = false;
    QString conflictName;
    for (const QString& name : m_clipboardPaths) {
        if (FileManager::instance()->nameExists(name)) {
            hasConflict = true;
            conflictName = name;
            break;
        }
    }

    if (hasConflict) {
        QMessageBox::StandardButton reply =
            QMessageBox::question(this, "同名冲突", QString("目标路径已存在 \"%1\"，是否覆盖？").arg(conflictName), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            ui->statusLabel->setText("粘贴已取消");
            return;
        }
    }

    // 执行复制/移动
    for (const QString& name : m_clipboardPaths) {
        // 源路径 = 剪贴板记录的目录 + 每一个文件名
        QString sourcePath = m_clipboardSourceDir;
        if (!sourcePath.endsWith("/")) sourcePath += "/";
        sourcePath += name;

        // 构建目标路径（绝对路径）
        QString targetPath = buildPath(name);

        if (m_clipboardAction == "copy") {
            // 调用网络端的复制文件
            NetworkManager::instance()->copyFile(sourcePath, targetPath);
        } else if (m_clipboardAction == "cut") {
            // 如果同目录则不操作
            if (m_clipboardSourceDir == currentDirectory) {
                continue;
            }
            // 剪切，即移动文件
            NetworkManager::instance()->moveFile(sourcePath, targetPath);
        }
    }

    if (m_clipboardAction == "copy") {
        ui->statusLabel->setText("正在复制...");
    } else {
        ui->statusLabel->setText("正在移动...");
        m_clipboardAction = "";
        m_clipboardPaths.clear();
    }

    // 粘贴后立即刷新列表
    NetworkManager::instance()->listFiles(currentDirectory);
    // 刷新用户空间占用
    refreshUserInfo();
}

// ===== 账户管理 =====

void MainWindow::on_actionUpdateAvatar_triggered() {
    QString filePath = QFileDialog::getOpenFileName(this, "选择头像图片", QDir::homePath(), "图片文件 (*.png *.jpg *.jpeg *.bmp)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ui->statusLabel->setText("无法打开头像文件");
        return;
    }
    QByteArray imageData = file.readAll();
    file.close();

    // 限制头像大小 4MB
    if (imageData.size() > 4 * 1024 * 1024) {
        ui->statusLabel->setText("头像文件不能超过 4MB");
        file.close();
        return;
    }

    NetworkManager::instance()->updateAvatar(imageData);
    ui->statusLabel->setText("正在更新头像...");
}

void MainWindow::on_actionUpdateNickname_triggered() {
    bool ok;
    QString nickname = QInputDialog::getText(this, "修改昵称", "请输入新昵称:", QLineEdit::Normal, m_currentUser.nickname(), &ok);
    if (!ok || nickname.isEmpty()) return;

    if (nickname.length() > 50) {
        ui->statusLabel->setText("昵称不能超过 50 个字符");
        return;
    }

    NetworkManager::instance()->updateNickname(nickname);
    ui->statusLabel->setText("正在更新昵称...");
}

void MainWindow::on_actionUpdateEmail_triggered() {
    bool ok;
    QString email = QInputDialog::getText(this, "修改邮箱", "请输入新邮箱:", QLineEdit::Normal, m_currentUser.email(), &ok);
    if (!ok || email.isEmpty()) return;

    // 简单邮箱格式校验
    if (!email.contains('@') || !email.contains('.')) {
        ui->statusLabel->setText("邮箱格式不正确");
        return;
    }

    NetworkManager::instance()->updateEmail(email);
    ui->statusLabel->setText("正在更新邮箱...");
}

void MainWindow::on_actionChangePassword_triggered() {
    // 弹出修改密码对话框
    QDialog dlg(this);
    dlg.setWindowTitle("修改密码");
    QFormLayout form(&dlg);

    // 新建密码输入框：旧密码、新密码、确认新密码
    QLineEdit* oldPw = new QLineEdit(&dlg);
    oldPw->setEchoMode(QLineEdit::Password);
    oldPw->setPlaceholderText("请输入旧密码");
    QLineEdit* newPw = new QLineEdit(&dlg);
    newPw->setEchoMode(QLineEdit::Password);
    newPw->setPlaceholderText("请输入新密码");
    QLineEdit* confirmPw = new QLineEdit(&dlg);
    confirmPw->setEchoMode(QLineEdit::Password);
    confirmPw->setPlaceholderText("请确认新密码");

    // 添加到表单布局
    form.addRow("旧密码:", oldPw);
    form.addRow("新密码:", newPw);
    form.addRow("确认密码:", confirmPw);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    // 如果用户点击取消，直接返回
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
        if (success) {
            QMessageBox::information(this, "注销成功", msg);
            emit logoutRequested();
        } else {
            QMessageBox::warning(this, "注销失败", msg);
        }
        disconnect(*conn);
    });
    NetworkManager::instance()->deleteUser();
}

void MainWindow::on_actionClearPreview_triggered() {
    QDir tempDir = QDir::temp();
    QStringList filters;
    filters << "*.txt" << "*.cpp" << "*.h" << "*.c" << "*.hpp" << "*.java"
            << "*.py" << "*.js" << "*.html" << "*.css" << "*.sh"
            << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif" << "*.pdf";
    // 只删除预览文件，避免误删其他临时文件
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
