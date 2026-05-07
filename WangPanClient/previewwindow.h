#ifndef PREVIEWWINDOW_H
#define PREVIEWWINDOW_H

#include <QMainWindow>

namespace Ui {
class PreviewWindow;
}

class PreviewWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PreviewWindow(QWidget *parent = nullptr);
    ~PreviewWindow();

    void setFile(const QString &filePath, const QString &fileName);

private slots:
    void on_closeButton_clicked();

private:
    Ui::PreviewWindow *ui;
    QString m_filePath;
    QString m_fileName;

    void previewTextFile();
    void previewImageFile();
    void previewPdfFile();
};

#endif // PREVIEWWINDOW_H