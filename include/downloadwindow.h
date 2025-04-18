#ifndef DOWNLOADWINDOW_H
#define DOWNLOADWINDOW_H
#include <QDialog>
#include <QThread>
#include "downloader.h"

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
    void onPauseResumeClicked();
    void onDownloadComplete(bool success);
    void onDownloadPaused();
    void onDownloadResumed();
    void updateUI(); // New slot to update UI periodically

private:
    Ui::DownloadWindow *ui;
    QThread* downloadThread;
    Downloader* downloader;
    bool isDownloading;
    
    void updateButtonStates();
};
#endif // DOWNLOADWINDOW_H