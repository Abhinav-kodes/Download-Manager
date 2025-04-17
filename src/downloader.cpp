#include "downloader.h"
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <QCoreApplication>

// Global handle for access in callbacks
CURL* g_curl = nullptr;

// WriteCallback function to write data to file
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* data = static_cast<std::pair<std::ofstream*, std::atomic<bool>*>*>(userp);
    std::ofstream* out = data->first;
    std::atomic<bool>* paused = data->second;
    
    // Check if download is paused before writing
    if (paused && paused->load() && g_curl) {
        std::cout << "WriteCallback detected pause, returning CURL_WRITEFUNC_PAUSE" << std::endl;
        return CURL_WRITEFUNC_PAUSE; // This will pause the transfer
    }
    
    out->write(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// Progress callback function to update the download progress
static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow) {
    auto* data = static_cast<std::pair<std::pair<std::function<void(int)>*, curl_off_t>*, std::atomic<bool>*>*>(clientp);
    auto* progressPair = data->first;
    auto* progressFunc = progressPair->first;
    curl_off_t resumePosition = progressPair->second;
    std::atomic<bool>* paused = data->second;
    
    // Update progress, accounting for already downloaded bytes
    if (progressFunc && dltotal > 0) {
        // Calculate based on total file size if known
        curl_off_t effectiveTotal = dltotal + resumePosition;
        curl_off_t effectiveNow = dlnow + resumePosition;
        int percent = static_cast<int>((effectiveNow * 100) / effectiveTotal);
        
        // Keep percent within valid range
        percent = std::min(100, std::max(0, percent));
        
        (*progressFunc)(percent); // Update progress with the correct percentage
    }
    
    // Check if download is paused
    if (paused && paused->load() && g_curl) {
        std::cout << "ProgressCallback detected pause" << std::endl;
        curl_easy_pause(g_curl, CURLPAUSE_ALL);
        return 1; // Return non-zero to abort current transfer
    }
    
    // Let the Qt event loop process events occasionally to keep UI responsive
    static int counter = 0;
    if (++counter % 10 == 0) { // Process events every 10 callbacks
        QCoreApplication::processEvents();
    }
    
    return 0; // Continue download
}

Downloader::Downloader(const std::string& url, const std::string& outputPath, std::function<void(int)> onProgress)
    : url(url), outputPath(outputPath), onProgress(onProgress), resumePosition(0), totalFileSize(0), running(false), paused(false) {
}

// Constructor for the Downloader class
bool Downloader::downloadFile(const std::string& url, const std::string& outputPath, std::function<void(int)> onProgress) {
    CURL* curl;
    CURLcode res;

    // Open file in appropriate mode based on resume position
    std::ios_base::openmode mode = std::ios::binary;
    if (resumePosition > 0) {
        mode |= std::ios::in | std::ios::out; // Open for update
    } else {
        mode |= std::ios::out; // Create new file
    }

    std::ofstream file(outputPath, mode);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << outputPath << std::endl;
        return false;
    }

    // Seek to resume position if needed
    if (resumePosition > 0) {
        file.seekp(resumePosition);
        std::cout << "Resuming download from position: " << resumePosition << std::endl;
    }

    // Get total file size first if resuming
    if (resumePosition > 0 && totalFileSize == 0) {
        std::ifstream checkFile(outputPath, std::ios::binary | std::ios::ate);
        if (checkFile.good()) {
            totalFileSize = std::max(totalFileSize, (curl_off_t)checkFile.tellg());
        }
    }

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    // Store in global variable for access from callbacks
    g_curl = curl;

    if (!curl) {
        std::cerr << "Failed to initialize cURL." << std::endl;
        file.close();
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    // Create a pair to pass both the file and the paused flag
    std::pair<std::ofstream*, std::atomic<bool>*> fileData(&file, &paused);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fileData);

    // Set resume position if needed
    if (resumePosition > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, resumePosition);
    }

    // Create a nested pair to pass progress function + resume position + paused flag
    std::pair<std::function<void(int)>*, curl_off_t> progressPair(&onProgress, resumePosition);
    std::pair<std::pair<std::function<void(int)>*, curl_off_t>*, std::atomic<bool>*> progressData(&progressPair, &paused);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressData);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");

    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 8192L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 3L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 0L);

    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    errbuf[0] = 0;

    running.store(true);

    std::atomic<bool> shouldAbort(false);
    std::thread monitorThread([this, curl, &shouldAbort]() {
        while (running.load() && !shouldAbort.load()) {
            if (paused.load()) {
                std::cout << "Monitor thread detected pause state, calling curl_easy_pause()" << std::endl;
                curl_easy_pause(curl, CURLPAUSE_ALL);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            QCoreApplication::processEvents();
        }
    });

    res = curl_easy_perform(curl);

    shouldAbort.store(true);
    if (monitorThread.joinable()) {
        monitorThread.join();
    }

    curl_off_t downloadedSize = 0;
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &downloadedSize);

    if (downloadedSize > 0) {
        resumePosition += downloadedSize;
        std::cout << "Downloaded size: " << downloadedSize << ", new resume position: " << resumePosition << std::endl;
    }

    // Update total file size after download
    curl_off_t dlTotal = 0;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &dlTotal);
    if (dlTotal > 0) {
        totalFileSize = resumePosition + dlTotal;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    g_curl = nullptr;

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    file.close();
    running.store(false);

    if (paused.load() || 
        res == CURLE_WRITE_ERROR || 
        res == CURLE_ABORTED_BY_CALLBACK || 
        res == CURLE_OPERATION_TIMEDOUT || 
        res == CURLE_PARTIAL_FILE) {
        std::cout << "Download paused or aborted at position: " << resumePosition << std::endl;
        return false;
    }

    if (res != CURLE_OK) {
        std::cerr << "Download failed: " << curl_easy_strerror(res) << std::endl;
        if (errbuf[0] != '\0') {
            std::cerr << "Error details: " << errbuf << std::endl;
        }
        std::cerr << "HTTP response code: " << http_code << std::endl;
        return false;
    }

    std::cout << "Download completed successfully! HTTP code: " << http_code << std::endl;
    return true;
}

// Slot to start the download process
void Downloader::startDownload() {
    paused.store(false);
    resumePosition = 0; // Start from beginning
    totalFileSize = 0;  // Reset total file size
    
    // Optionally: Make a HEAD request to get total file size before downloading
    CURL* curlHead = curl_easy_init();
    if (curlHead) {
        curl_easy_setopt(curlHead, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curlHead, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curlHead, CURLOPT_HEADER, 1L);
        curl_easy_setopt(curlHead, CURLOPT_FOLLOWLOCATION, 1L);
        
        if (curl_easy_perform(curlHead) == CURLE_OK) {
            curl_easy_getinfo(curlHead, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &totalFileSize);
            std::cout << "Total file size: " << totalFileSize << std::endl;
        }
        curl_easy_cleanup(curlHead);
    }
    
    bool result = downloadFile(url, outputPath, onProgress);
    emit downloadFinished(result);
}
// Slot to pause the download
void Downloader::pauseDownload() {
    std::cout << "Pause requested" << std::endl;
    if (running.load()) {
        std::cout << "Setting paused flag to true" << std::endl;
        paused.store(true);
        
        // If we have a valid curl handle, try to pause it directly
        if (g_curl) {
            std::cout << "Calling curl_easy_pause() directly from pauseDownload()" << std::endl;
            curl_easy_pause(g_curl, CURLPAUSE_ALL);
        }
        
        emit downloadPaused();
    }
}
// Slot to resume the download
void Downloader::resumeDownload() {
    std::cout << "Resume requested" << std::endl;
    if (!running.load() && paused.load()) {
        paused.store(false);
        
        // Update progress to show current status before resuming
        if (onProgress && totalFileSize > 0) {
            int currentPercent = static_cast<int>((resumePosition * 100) / totalFileSize);
            onProgress(currentPercent);
        }
        
        emit downloadResumed();
        bool result = downloadFile(url, outputPath, onProgress);
        if (!paused.load()) {
            emit downloadFinished(result);
        }
    }
}

bool Downloader::isPaused() const {
    return paused.load();
}