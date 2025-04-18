#include "downloadwindow.h"
#include "ui_downloadwindow.h"
#include "downloader.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QThread>
#include <QTimer>
#include <iostream>

DownloadWindow::DownloadWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DownloadWindow)
    , downloadThread(nullptr)
    , downloader(nullptr)
    , isDownloading(false)
{
    ui->setupUi(this);
    setWindowTitle("Download Manager");
    
    // Connect buttons
    connect(ui->downloadButton, &QPushButton::clicked, this, &DownloadWindow::onDownloadClicked);
    connect(ui->pauseResumeButton, &QPushButton::clicked, this, &DownloadWindow::onPauseResumeClicked);
    
    // Initially disable pause/resume button
    ui->pauseResumeButton->setEnabled(false);
    ui->pauseResumeButton->setText("Pause");
    
    // Create a timer to update the UI regularly
    QTimer *progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &DownloadWindow::updateUI);
    progressTimer->start(100); // Update every 100ms
}

DownloadWindow::~DownloadWindow()
{
    // Request the thread to quit and wait for it to finish
    if (downloadThread && downloadThread->isRunning()) {
        // Optionally, try to pause first if applicable and safe
        // if (downloader) {
        //     QMetaObject::invokeMethod(downloader, "pauseDownload", Qt::BlockingQueuedConnection); // Blocking might be needed here
        // }
        downloadThread->quit(); // Request event loop termination
        if (!downloadThread->wait(3000)) { // Wait up to 3 seconds
            // If wait times out, consider logging an error.
            // Terminate is a last resort as it can lead to resource leaks.
            std::cerr << "Warning: Download thread did not finish gracefully. Terminating." << std::endl;
            downloadThread->terminate();
            downloadThread->wait(); // Wait after terminate
        }
        // Do not manually delete downloader or downloadThread here.
        // Rely on the deleteLater connections made in onDownloadClicked.
    }
    // Note: downloader pointer might be dangling here if thread terminated forcefully.
    // It's generally safer if the thread finishes cleanly.

    delete ui; // Standard Qt practice for UI cleanup
}

void DownloadWindow::updateUI()
{
    if (!isDownloading || !downloader) return;
    
    // Update button text and state based on download state
    if (downloader->isPaused()) {
        ui->pauseResumeButton->setText("Resume");
        ui->pauseResumeButton->setEnabled(true);
    } else {
        ui->pauseResumeButton->setText("Pause");
        ui->pauseResumeButton->setEnabled(true);
    }
    
    // Process events to keep UI responsive
    QCoreApplication::processEvents();
}

void DownloadWindow::updateButtonStates() {
    if (isDownloading) {
        ui->downloadButton->setEnabled(false);
        // Enable pause/resume button only if downloader exists
        ui->pauseResumeButton->setEnabled(downloader != nullptr);

        // Check the actual state via isPaused()
        if (downloader && downloader->isPaused()) {
            ui->pauseResumeButton->setText("Resume");
        } else {
            ui->pauseResumeButton->setText("Pause");
        }
    } else {
        ui->downloadButton->setEnabled(true);
        ui->pauseResumeButton->setEnabled(false);
        ui->pauseResumeButton->setText("Pause"); // Reset text when not downloading
    }
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

    // --- Cleanup existing thread and downloader first ---
    if (downloadThread) {
        if (downloadThread->isRunning()) {
            std::cout << "Cleaning up previous download thread..." << std::endl;
            // Request stop/pause if downloader exists
            if (downloader) {
                 // Use BlockingQueuedConnection if you need to ensure pause happens before quit
                 // QMetaObject::invokeMethod(downloader, "pauseDownload", Qt::BlockingQueuedConnection);
                 // Or just rely on quit() stopping processing
            }
            downloadThread->quit();
            if (!downloadThread->wait(1000)) { // Wait briefly
                 std::cerr << "Warning: Previous download thread did not finish gracefully during cleanup." << std::endl;
                 // Avoid terminate here if possible, let deleteLater handle it eventually
            }
            // The finished signal should trigger deleteLater for the old objects
        } else {
            // If thread wasn't running, maybe it finished but pointers weren't cleared?
            // Or maybe deleteLater hasn't executed yet. It's usually safe to just null pointers.
        }
        // Clear pointers immediately after initiating cleanup
        downloader = nullptr;
        downloadThread = nullptr;
    }
    // --- End cleanup ---

    // Create a lambda to update the progress bar
    auto updateProgress = [this](int percent) {
        // Use invokeMethod with QueuedConnection to ensure UI updates happen in the UI thread
        QMetaObject::invokeMethod(this, [this, percent]() {
            // Check if the progress bar still exists (window might be closing)
            if (ui && ui->progressBar) {
                ui->progressBar->setValue(percent);
            }
            // Avoid explicit QCoreApplication::processEvents() here if possible,
            // rely on the main event loop and the timer.
        }, Qt::QueuedConnection);
    };

    // Create a new thread with this window as parent
    downloadThread = new QThread(this); // Set parent
    downloader = new Downloader(url.toStdString(), output.toStdString(), updateProgress);
    downloader->moveToThread(downloadThread);

    // --- Connections ---
    // Start download when thread starts
    connect(downloadThread, &QThread::started, downloader, &Downloader::startDownload);

    // Ensure downloader is deleted when thread finishes
    connect(downloadThread, &QThread::finished, downloader, &QObject::deleteLater);

    // Ensure thread deletes itself when finished
    connect(downloadThread, &QThread::finished, downloadThread, &QObject::deleteLater);

    // Connect downloader signals to window slots (ensure QueuedConnection for thread safety)
    connect(downloader, &Downloader::downloadFinished, this, &DownloadWindow::onDownloadComplete, Qt::QueuedConnection);
    connect(downloader, &Downloader::downloadPaused, this, &DownloadWindow::onDownloadPaused, Qt::QueuedConnection);
    connect(downloader, &Downloader::downloadResumed, this, &DownloadWindow::onDownloadResumed, Qt::QueuedConnection);

    // --- Start Download ---
    isDownloading = true;
    updateButtonStates();
    downloadThread->start();
}

void DownloadWindow::onPauseResumeClicked() {
    std::cout << "Pause/Resume button clicked" << std::endl;
    if (!downloader) return;

    // Temporarily disable button until action completes or state updates
    // ui->pauseResumeButton->setEnabled(false); // Let updateButtonStates handle this

    if (downloader->isPaused()) {
        std::cout << "Calling resumeDownload via invokeMethod" << std::endl;
        // Resume still needs invokeMethod as it might call downloadFile()
        QMetaObject::invokeMethod(downloader, "resumeDownload", Qt::QueuedConnection);

        // Optimistically update UI for resume, updateButtonStates will confirm
        // ui->pauseResumeButton->setText("Pause");
        // ui->pauseResumeButton->setEnabled(true); // Let updateButtonStates handle enabling
    } else {
        std::cout << "Calling requestPause directly" << std::endl;
        // Directly call the new method to set the atomic flag immediately
        downloader->requestPause();

        // Optimistically update UI for pause, updateButtonStates will confirm
        // ui->pauseResumeButton->setText("Resume");
        // ui->pauseResumeButton->setEnabled(true); // Let updateButtonStates handle enabling
    }
    // Update button states immediately based on the requested action
    // updateButtonStates(); // Removed this immediate call
}

void DownloadWindow::onDownloadComplete(bool success) {
    // Check if the 'failure' was actually due to a pause request
    // Ensure downloader pointer is valid before checking isPaused
    if (!success && downloader && downloader->isPaused()) {
        // This wasn't a real failure, it was a pause stopping the transfer.
        // The UI should already be reflecting the 'Paused' state (handled by onDownloadPaused).
        // We should not set isDownloading to false or update button states here.
        std::cout << "onDownloadComplete: Ignoring 'false' success because download is paused." << std::endl;
        return; // Exit early, leave UI in paused state
    }

    // If it's a real completion (success or actual failure, not a pause)
    isDownloading = false;
    updateButtonStates(); // Update UI to reflect the truly stopped state

    if (success) {
        QMessageBox::information(this, "Download Complete",
                                "The file has been downloaded successfully.");
    } else {
        // We already handled the pause case above, so this is a genuine failure.
        QMessageBox::critical(this, "Download Failed",
                             "There was an error downloading the file.");
    }
}

void DownloadWindow::onDownloadPaused() {
    // Use invokeMethod to ensure UI updates happen in the UI thread
    QMetaObject::invokeMethod(this, [this]() {
        std::cout << "Download paused, updating UI" << std::endl;
        // QMessageBox::information(this, "Download Paused",
        //                       "The download has been paused. Click Resume to continue."); // Removed this line

        // Update button states AFTER the message box is closed (or would have been)
        ui->pauseResumeButton->setText("Resume");
        ui->pauseResumeButton->setEnabled(true);

        // Process events to ensure UI updates
        QCoreApplication::processEvents();
    }, Qt::QueuedConnection);
}

void DownloadWindow::onDownloadResumed() {
    // Use invokeMethod to ensure UI updates happen in the UI thread
    QMetaObject::invokeMethod(this, [this]() {
        ui->pauseResumeButton->setText("Pause");
        ui->pauseResumeButton->setEnabled(true);
        std::cout << "Download resumed, updating UI" << std::endl;
        
        // Process events to ensure UI updates
        QCoreApplication::processEvents();
    }, Qt::QueuedConnection);
}