#include "previewwindow.h"

#include <QDesktopServices>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "ui_previewwindow.h"

PreviewWindow::PreviewWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::PreviewWindow), m_imageZoom(1.0), m_imageViewLabel(nullptr) { ui->setupUi(this); }

PreviewWindow::~PreviewWindow() { delete ui; }

void PreviewWindow::setFile(const QString& filePath, const QString& fileName) {
    m_filePath = filePath;
    m_fileName = fileName;
    ui->fileNameLabel->setText(fileName);

    // 根据文件类型选择预览方式
    QString extension = fileName.split('.').last().toLower();
    if (extension == "txt" || extension == "cpp" || extension == "h" || extension == "c" || extension == "hpp" || extension == "java" || extension == "py" || extension == "js" ||
        extension == "html" || extension == "css" || extension == "sh") {
        previewTextFile();
        ui->previewTabWidget->setCurrentWidget(ui->textTab);
    } else if (extension == "jpg" || extension == "jpeg" || extension == "png" || extension == "bmp" || extension == "gif") {
        previewImageFile();
        ui->previewTabWidget->setCurrentWidget(ui->imageTab);
    } else if (extension == "pdf") {
        previewPdfFile();
        ui->previewTabWidget->setCurrentWidget(ui->pdfTab);
    } else {
        // 不支持的文件类型
        ui->textEdit->setText("不支持的文件类型");
        ui->previewTabWidget->setCurrentWidget(ui->textTab);
    }
}

void PreviewWindow::previewTextFile() {
    QFile file(m_filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString content = in.readAll();
        ui->textEdit->setPlainText(content);
        file.close();
    } else {
        ui->textEdit->setText("无法打开文件");
    }
}

void PreviewWindow::previewImageFile() {
    // 清理旧布局
    if (ui->imageTab->layout()) {
        QLayoutItem* child;
        while ((child = ui->imageTab->layout()->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
    }

    QImage image(m_filePath);
    if (image.isNull()) {
        ui->imageTab->layout()->addWidget(new QLabel("无法加载图片", ui->imageTab));
        return;
    }

    m_originalPixmap = QPixmap::fromImage(image);
    m_imageZoom = 1.0;
    m_imageViewLabel = nullptr;

    // 创建 QScrollArea 包含 QLabel，支持滚轮缩放
    auto* scrollArea = new QScrollArea(ui->imageTab);
    scrollArea->setWidgetResizable(false);
    scrollArea->setAlignment(Qt::AlignCenter);

    auto* label = new QLabel;
    label->setAlignment(Qt::AlignCenter);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    scrollArea->setWidget(label);

    ui->imageTab->layout()->addWidget(scrollArea);

    // 初次显示：缩放到适合窗口大小
    QSize viewSize = scrollArea->viewport()->size();
    if (viewSize.isEmpty()) viewSize = QSize(780, 480);
    m_imageZoom = qMin(static_cast<double>(viewSize.width()) / m_originalPixmap.width(), static_cast<double>(viewSize.height()) / m_originalPixmap.height());
    if (m_imageZoom > 1.0) m_imageZoom = 1.0;  // 不要放大超过原始尺寸
    m_imageViewLabel = label;
    updateImageDisplay();
}

void PreviewWindow::updateImageDisplay() {
    if (!m_imageViewLabel || m_originalPixmap.isNull()) return;
    QSize newSize = m_originalPixmap.size() * m_imageZoom;
    m_imageViewLabel->setPixmap(m_originalPixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imageViewLabel->resize(newSize);
}

void PreviewWindow::wheelEvent(QWheelEvent* event) {
    // 仅当在图片标签页且图片已加载时处理滚轮缩放
    if (ui->previewTabWidget->currentWidget() == ui->imageTab && m_imageViewLabel) {
        double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        double newZoom = m_imageZoom * factor;
        if (newZoom >= 0.05 && newZoom <= 10.0) {
            m_imageZoom = newZoom;
            updateImageDisplay();
        }
        event->accept();
        return;
    }
    QMainWindow::wheelEvent(event);
}

void PreviewWindow::previewPdfFile() {
    // 清理旧子控件和布局
    if (ui->pdfWidget->layout()) {
        QLayoutItem* child;
        while ((child = ui->pdfWidget->layout()->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
        delete ui->pdfWidget->layout();
    }

    auto* vbox = new QVBoxLayout(ui->pdfWidget);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->addStretch();

    auto* infoLabel = new QLabel("PDF 预览需借助外部应用", ui->pdfWidget);
    infoLabel->setAlignment(Qt::AlignCenter);
    infoLabel->setStyleSheet("font-size: 14px; color: #666;");
    vbox->addWidget(infoLabel);

    auto* openBtn = new QPushButton("用系统默认应用打开 PDF", ui->pdfWidget);
    vbox->addWidget(openBtn, 0, Qt::AlignCenter);
    connect(openBtn, &QPushButton::clicked, this, [=]() { QDesktopServices::openUrl(QUrl::fromLocalFile(m_filePath)); });

    vbox->addStretch();
}

void PreviewWindow::on_closeButton_clicked() { this->close(); }
