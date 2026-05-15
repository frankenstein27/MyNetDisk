#ifndef PREVIEWWINDOW_H
#define PREVIEWWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QPixmap>

namespace Ui {
class PreviewWindow;
}

class PreviewWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit PreviewWindow(QWidget* parent = nullptr);
    ~PreviewWindow();

    void setFile(const QString& filePath, const QString& fileName);

   protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

   private slots:
    void on_closeButton_clicked();

   private:
    Ui::PreviewWindow* ui;
    QString m_filePath;
    QString m_fileName;

    // 图片缩放
    QPixmap m_originalPixmap;
    double m_imageZoom;
    QLabel* m_imageViewLabel;  // QScrollArea 内的实际图片标签
    void updateImageDisplay();

    void previewTextFile();
    void previewImageFile();
    void previewPdfFile();
};

#endif  // PREVIEWWINDOW_H
