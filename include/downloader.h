#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QObject>
#include <string>
#include <functional>
#include <atomic>
#include <curl/curl.h>

class Downloader : public QObject {
    Q_OBJECT
public:
    // Constructor
    Downloader(const std::string& url, const std::string& outputPath, std::function<void(int)> onProgress);
    // Check if download is paused
    bool isPaused() const;
    // New method to request pause directly (thread-safe due to atomic flag)
    void requestPause();

public slots:
    // Slot to start the download
    void startDownload();
    // Slot to pause the download (can potentially be removed if not used elsewhere)

    // Slot to resume the download
    void resumeDownload();

signals:
    // Signal when download is finished
    void downloadFinished(bool success);
    // Signal when download is paused
    void downloadPaused();
    // Signal when download is resumed
    void downloadResumed();

private:
    std::string url;
    std::string outputPath;
    std::function<void(int)> onProgress;
    std::atomic<bool> paused;
    std::atomic<bool> running;
    curl_off_t resumePosition;
    curl_off_t totalFileSize;
    CURL* m_curlHandle;
    // Function to handle the file download
    bool downloadFile();
};

#endif // DOWNLOADER_H