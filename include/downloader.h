#ifndef DOWNLOADER_H
#define DOWNLOADER_H
#include <QObject>
#include <string>
#include <functional>

class Downloader : public QObject {
    Q_OBJECT

public:
    // Constructor
    Downloader(const std::string& url, const std::string& outputPath, std::function<void(int)> onProgress);

public slots:
    // Slot to start the download
    void startDownload();

signals:
    // Signal when download is finished
    void downloadFinished(bool success);

private:
    std::string url;
    std::string outputPath;
    std::function<void(int)> onProgress;

    // Function to handle the file download
    bool downloadFile(const std::string& url, const std::string& outputPath, std::function<void(int)> onProgress);
};

#endif // DOWNLOADER_H