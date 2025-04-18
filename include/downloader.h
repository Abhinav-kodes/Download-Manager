#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QObject>
#include <string>
#include <functional>
#include <atomic>
#include <curl/curl.h> // Add this include for curl_off_t

class Downloader : public QObject {
    Q_OBJECT
public:
    // Constructor
    Downloader(const std::string& url, const std::string& outputPath, std::function<void(int)> onProgress);

    // Check if download is paused
    bool isPaused() const;

public slots:
    // Slot to start the download
    void startDownload();
    
    // Slot to pause the download
    void pauseDownload();
    
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
    curl_off_t totalFileSize;  // Track total file size
    CURL* m_curlHandle;
    // Function to handle the file download
    bool downloadFile();
};

#endif // DOWNLOADER_H