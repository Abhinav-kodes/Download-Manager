#include "downloadwindow.h"
#include "ui_downloadwindow.h"
#include "downloader.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QThread>  // Include to use QThread

DownloadWindow::DownloadWindow(QWidget *parent)
    : QDialog(parent) // Changed from QWidget to QDialog
    , ui(new Ui::DownloadWindow)
{
    ui->setupUi(this);
    setWindowTitle("Download Manager");
    connect(ui->downloadButton, &QPushButton::clicked,
            this, &DownloadWindow::onDownloadClicked);
}

DownloadWindow::~DownloadWindow()
{
    delete ui;
}

void DownloadWindow::onDownloadClicked() {

    QString url = ui->urlLineEdit->text();

   

    if (url.isEmpty()) {

        QMessageBox::warning(this, "Error", "Please enter a URL.");

        return;

    }

   

    // Ensure the URL has a protocol (http:// or https://)

    if (!url.startsWith("http://") && !url.startsWith("https://")) {

        url = "https://" + url;

    }

   

    // Extract filename from URL and preserve extension

    QString defaultName = url.split("/").last();

    if (defaultName.isEmpty() || !defaultName.contains(".")) {

        // Try to guess file type from URL or default to generic name

        if (url.contains(".pdf", Qt::CaseInsensitive)) {

            defaultName = "download.pdf";

        } else if (url.contains(".jpg", Qt::CaseInsensitive) ||

                  url.contains(".jpeg", Qt::CaseInsensitive)) {

            defaultName = "download.jpg";

        } else if (url.contains(".png", Qt::CaseInsensitive)) {

            defaultName = "download.png";

        } else {

            defaultName = "download.html";

        }

    }

   

    // Create filter based on file extension

    QString filter = "All Files (*.*)";

    if (defaultName.contains(".")) {

        QString ext = defaultName.split(".").last();

        if (!ext.isEmpty()) {

            filter = QString("%1 Files (*.%2);;All Files (*.*)").arg(ext.toUpper()).arg(ext);

        }

    }

   

    QString output = QFileDialog::getSaveFileName(this, "Save File",

                                                 defaultName,

                                                 filter);

   

    if (output.isEmpty()) return; // User canceled

   

    ui->progressBar->setValue(0);  // Reset progress


    // Create a lambda to update the progress bar

    auto updateProgress = [this](int percent) {

        QMetaObject::invokeMethod(this, [this, percent]() {

            ui->progressBar->setValue(percent);

        }, Qt::QueuedConnection);

    };


    // Create a new thread to handle the download

    QThread* downloadThread = new QThread();

    Downloader* downloader = new Downloader(url.toStdString(), output.toStdString(), updateProgress);

    downloader->moveToThread(downloadThread);


    connect(downloadThread, &QThread::started, downloader, &Downloader::startDownload);

    connect(downloader, &Downloader::downloadFinished, downloadThread, &QThread::quit);

    connect(downloader, &Downloader::downloadFinished, downloader, &Downloader::deleteLater);

    connect(downloadThread, &QThread::finished, downloadThread, &QThread::deleteLater);

    connect(downloader, &Downloader::downloadFinished, this, &DownloadWindow::onDownloadComplete);

    downloadThread->start();

}


void DownloadWindow::onDownloadComplete(bool success) {

    if (success) {

        QMessageBox::information(this, "Download Complete",

                                "The file has been downloaded successfully.");

    } else {

        QMessageBox::critical(this, "Download Failed",

                             "There was an error downloading the file.");

    }

}

