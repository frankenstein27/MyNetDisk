#include "previewwindow.h"
#include "ui_previewwindow.h"
#include <QFile>
#include <QTextStream>
#include <QImage>
#include <QPixmap>

PreviewWindow::PreviewWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::PreviewWindow)
{
    ui->setupUi(this);
}

PreviewWindow::~PreviewWindow()
{
    delete ui;
}

void PreviewWindow::setFile(const QString &filePath, const QString &fileName)
{
    m_filePath = filePath;
    m_fileName = fileName;
    ui->fileNameLabel->setText(fileName);

    // 根据文件类型选择预览方式
    QString extension = fileName.split('.').last().toLower();
    if (extension == "txt" || extension == "cpp" || extension == "h" || extension == "c" || extension == "hpp" || extension == "java" || extension == "py" || extension == "js" || extension == "html" || extension == "css" || extension == "sh") {
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

void PreviewWindow::previewTextFile()
{
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

void PreviewWindow::previewImageFile()
{
    QImage image(m_filePath);
    if (!image.isNull()) {
        QPixmap pixmap = QPixmap::fromImage(image);
        pixmap = pixmap.scaled(ui->imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->imageLabel->setPixmap(pixmap);
    } else {
        ui->imageLabel->setText("无法加载图片");
    }
}

void PreviewWindow::previewPdfFile()
{
    // 这里可以集成PDF预览库，如Poppler
    ui->pdfWidget->setStyleSheet("background-color: #f0f0f0;");
    // 暂时显示提示信息
    QLabel *label = new QLabel(ui->pdfWidget);
    label->setText("PDF预览功能需要集成PDF库");
    label->setAlignment(Qt::AlignCenter);
    QVBoxLayout *layout = new QVBoxLayout(ui->pdfWidget);
    layout->addWidget(label);
}

void PreviewWindow::on_closeButton_clicked()
{
    this->close();
}
