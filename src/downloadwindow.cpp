#include "downloadwindow.h"
#include "ui_downloadwindow.h"
#include "downloader.h"
#include <QMessageBox>
#include <QFileDialog>

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
    
    bool result = downloadFile(url.toStdString(), output.toStdString());
    if (result) {
        QMessageBox::information(this, "Success", "Download completed!");
    } else {
        QMessageBox::critical(this, "Failed", "Download failed. Check console for details.");
    }
}