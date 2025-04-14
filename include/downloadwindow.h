#ifndef DOWNLOADWINDOW_H
#define DOWNLOADWINDOW_H
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class DownloadWindow; }
QT_END_NAMESPACE

class DownloadWindow : public QDialog {
    Q_OBJECT
public:
    explicit DownloadWindow(QWidget *parent = nullptr);
    ~DownloadWindow();

private slots:
    void onDownloadClicked();

private:
    Ui::DownloadWindow *ui;
};
#endif // DOWNLOADWINDOW_H