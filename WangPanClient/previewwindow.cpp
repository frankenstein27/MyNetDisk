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
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "ui_previewwindow.h"

// 后缀为以下类型可以以文本方式预览
static const QSet<QString> s_PreviewAsTxt = {"txt", "cpp", "h", "c", "hpp", "java", "py", "js", "html", "css"};
// 图像
static const QSet<QString> s_PreviewAsImage = {"jpg", "jpeg", "png", "bmp", "gif"};

PreviewWindow::PreviewWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::PreviewWindow), m_imageZoom(1.0), m_imageViewLabel(nullptr) { ui->setupUi(this); }

PreviewWindow::~PreviewWindow() { delete ui; }

void PreviewWindow::setFile(const QString& filePath, const QString& fileName) {
    m_filePath = filePath;
    m_fileName = fileName;
    ui->fileNameLabel->setText(fileName);

    // 根据文件类型选择预览方式
    QString extension = fileName.split('.').last().toLower();
    if (s_PreviewAsTxt.contains(extension)) {
        previewTextFile();
        ui->previewTabWidget->setCurrentWidget(ui->textTab);
    } else if (s_PreviewAsImage.contains(extension)) {
        previewImageFile();
        ui->previewTabWidget->setCurrentWidget(ui->imageTab);
    } else if (extension == "pdf") {
        previewPdfFile();
        ui->previewTabWidget->setCurrentWidget(ui->pdfTab);
    } else {
        // 不支持预览的文件类型，使用系统默认应用打开
        ui->previewTabWidget->setCurrentWidget(ui->textTab);

        // 清除旧的文本编辑内容
        ui->textEdit->clear();

        // 显示提示信息
        QString extUpper = extension.toUpper();
        ui->textEdit->setText("文件类型: " + extUpper + "\n\n该文件类型不支持内置预览。\n\n正在使用系统默认应用打开...");

        // 使用系统默认应用打开文件
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_filePath));
    }
}

// 打开要预览的文本文件，并显示在文本编辑器中
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

// 打开要预览的图片文件，并显示在标签中
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

    // 安装事件过滤器以截获 scrollArea（含 viewport）上的滚轮事件
    scrollArea->viewport()->installEventFilter(this);
    scrollArea->installEventFilter(this);

    m_imageViewLabel = label;

    // 让布局先完成，然后用 QTimer::singleShot 计算合适缩放
    QTimer::singleShot(0, this, [this, scrollArea]() {
        if (!m_imageViewLabel || m_originalPixmap.isNull()) return;
        QSize viewSize = scrollArea->viewport()->size();
        if (viewSize.isEmpty()) viewSize = QSize(780, 480);
        m_imageZoom = qMin(static_cast<double>(viewSize.width()) / m_originalPixmap.width(), static_cast<double>(viewSize.height()) / m_originalPixmap.height());
        if (m_imageZoom < 0.05) m_imageZoom = 0.05;
        if (m_imageZoom > 1.0) m_imageZoom = 1.0;
        updateImageDisplay();
    });
}

void PreviewWindow::updateImageDisplay() {
    if (!m_imageViewLabel || m_originalPixmap.isNull()) return;
    QSize newSize = m_originalPixmap.size() * m_imageZoom;
    m_imageViewLabel->setPixmap(m_originalPixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imageViewLabel->resize(newSize);
}

bool PreviewWindow::eventFilter(QObject* obj, QEvent* event) {
    // 截获 QScrollArea（及其内部 viewport）上的滚轮事件，用于图片缩放
    if (event->type() == QEvent::Wheel && ui->previewTabWidget->currentWidget() == ui->imageTab && m_imageViewLabel) {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        double factor = (wheelEvent->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        double newZoom = m_imageZoom * factor;
        if (newZoom >= 0.05 && newZoom <= 10.0) {
            m_imageZoom = newZoom;
            updateImageDisplay();
        }
        return true;  // 消费事件，阻止 QScrollArea 将其用于滚动
    }
    return QMainWindow::eventFilter(obj, event);
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
