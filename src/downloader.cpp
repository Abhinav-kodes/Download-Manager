#include "downloader.h"
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <QCoreApplication>
#include <functional>
#include <QDir>
#include <string>

struct CurlCallbackContext {
    std::ofstream* fileStream = nullptr;            // Pointer to the output file stream
    std::atomic<bool>* pausedFlag = nullptr;        // Pointer to the shared pause flag
    std::function<void(int)>* progressFn = nullptr; // Pointer to the progress callback function object
    curl_off_t resumeOffset = 0;                    // Value of the resume position for this transfer
};

// WriteCallback function to write data to file
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        // Cast userp to the context struct pointer type
        auto* context = static_cast<CurlCallbackContext*>(userp);
    
     // Check required pointers are valid before dereferencing (optional but safer)
     if (!context || !context->pausedFlag || !context->fileStream) {
        return CURL_WRITEFUNC_PAUSE; // Or another error signal, indicates setup issue
   }
    // Check if download is paused before writing
    if (context->pausedFlag->load()) {
        std::cout << "WriteCallback detected pause, returning CURL_WRITEFUNC_PAUSE" << std::endl;
        return CURL_WRITEFUNC_PAUSE; // This will pause the transfer
    }
    
    context->fileStream->write(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// Progress callback function to update the download progress
static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow) {
    // Cast clientp to the context struct pointer type
    auto* context = static_cast<CurlCallbackContext*>(clientp);

    // Check required pointers are valid (optional but safer)
    if (!context || !context->pausedFlag || !context->progressFn) {
    return 1; // Return non-zero to abort on setup issue
    }

    // Extract necessary data via the context struct
    std::function<void(int)>* progressFunc = context->progressFn;
    curl_off_t resumePosition = context->resumeOffset; // Use value from context
    std::atomic<bool>* paused = context->pausedFlag;   // Use pointer from context
    
     // Update progress, accounting for already downloaded bytes
     if (progressFunc && dltotal > 0) {
        curl_off_t effectiveTotal = dltotal + resumePosition;
        curl_off_t effectiveNow = dlnow + resumePosition;
        int percent = 0;
        // Avoid division by zero if effectiveTotal somehow becomes zero with positive dltotal
        if (effectiveTotal > 0) {
           percent = static_cast<int>((static_cast<double>(effectiveNow) * 100.0) / effectiveTotal);
        }

        percent = std::min(100, std::max(0, percent));

        // Call the progress function via the pointer stored in the context
        (*progressFunc)(percent);
    }
    
    // Check if download is paused
    if (paused->load()) { // Access paused flag via context pointer
        std::cout << "ProgressCallback detected pause" << std::endl;
        // DO NOT call curl_easy_pause here - just return non-zero
        return 1; // Return non-zero to abort current transfer
    }
    
    // Let the Qt event loop process events occasionally to keep UI responsive
    static int counter = 0;
    if (++counter % 10 == 0) { // Process events every 10 callbacks
        QCoreApplication::processEvents();
    }
    
    return 0; // Continue download
}

// Constructor for the Downloader class
Downloader::Downloader(const std::string& url, const std::string& outputPath, std::function<void(int)> onProgress)
    : QObject(nullptr),
      url(url),
      outputPath(outputPath),
      onProgress(onProgress),
      paused(false),
      running(false),
      resumePosition(0),
      totalFileSize(0),
      m_curlHandle(nullptr)
{
    // Constructor body
}

// downloadFile function updated to use member variables instead of parameters
bool Downloader::downloadFile() { // Parameters removed

    CURL* curl; // Use local variable for the handle within this function scope
    CURLcode res;

    // Open file in appropriate mode based on resume position
    std::ios_base::openmode mode = std::ios::binary;
    if (this->resumePosition > 0) { // Use member variable
        mode |= std::ios::in | std::ios::out; // Open for update
    } else {
        mode |= std::ios::out | std::ios::trunc; // Create/Truncate new file
    }

    // Use member variable for path
    std::ofstream file(this->outputPath, mode); // Use this->outputPath
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << this->outputPath << std::endl; // Use this->outputPath
        return false;
    }

    // Seek to resume position if needed
    if (this->resumePosition > 0) { // Use member variable
        file.seekp(this->resumePosition);
        std::cout << "Resuming download from position: " << this->resumePosition << std::endl;
    }

    curl = curl_easy_init(); // Initialize local handle

    if (!curl) { // Check initialization result immediately
        std::cerr << "Failed to initialize cURL." << std::endl;
        file.close();
        // No need to set m_curlHandle as it was never assigned
        return false;
    }

    this->m_curlHandle = curl; // Assign to member variable *after* successful init

    // --- Set Curl Options using Context Struct ---
    CurlCallbackContext callbackContext; // Define context struct
    callbackContext.fileStream = &file;
    callbackContext.pausedFlag = &this->paused;
    callbackContext.progressFn = &this->onProgress;
    callbackContext.resumeOffset = this->resumePosition;

    curl_easy_setopt(curl, CURLOPT_URL, this->url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &callbackContext); // Pass context to write callback
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &callbackContext); // Pass context to progress callback
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    if (callbackContext.resumeOffset > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, callbackContext.resumeOffset);
    }

    // --- Set other options ---
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Optional: for debugging
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 8192L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 3L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    // curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 0L); // Often default is fine

    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    errbuf[0] = 0;

    // --- Moved CA Certificate Handling Inside Function ---
    QString appDir = QCoreApplication::applicationDirPath();
    QString caCertPath = QDir(appDir).filePath("certs/cacert.pem");
    std::string caCertPathStd = caCertPath.toStdString();
    std::cout << "Attempting to use CA certificate file (relative path resolved to): "
              << caCertPathStd << std::endl;
    QFileInfo caCertInfo(caCertPath);
    if (!caCertInfo.exists() || !caCertInfo.isFile()) {
        std::cerr << "ERROR: CA certificate file not found at expected path: "
                  << caCertPathStd << std::endl;
        std::cerr << "Please ensure 'certs/cacert.pem' exists relative to the executable." << std::endl;
        // Clean up before returning failure
        file.close();
        curl_easy_cleanup(curl);
        this->m_curlHandle = nullptr;
        // No need to set running flag here as download hasn't started
        return false; // Fail the download explicitly if CA bundle is missing
    } else {
        curl_easy_setopt(curl, CURLOPT_CAINFO, caCertPathStd.c_str());
    }
    // --- End Moved CA Handling ---
    // --- Perform Download ---
    running.store(true); // Mark as running before starting
    res = curl_easy_perform(curl); // BLOCKING call
    running.store(false); // Mark as not running after completion or failure

    // --- Process Results ---
    // Get actual downloaded size for this specific transfer attempt
    curl_off_t downloadedSize = 0;
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &downloadedSize);

    // Get final HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // --- Cleanup ---
    this->m_curlHandle = nullptr; // Clear member handle pointer *before* cleanup
    curl_easy_cleanup(curl); // Clean up the local easy handle
    file.close(); // Close the file stream

    // --- Determine Return Status ---

    // Check if paused based on flag AND specific return codes from callbacks
    if (this->paused.load() &&
        (res == CURLE_WRITE_ERROR || res == CURLE_ABORTED_BY_CALLBACK)) {
         // Update resume position based on file size *after* pause detected
         std::ifstream checkFile(this->outputPath, std::ios::binary | std::ios::ate);
         if (checkFile.good()) {
             this->resumePosition = checkFile.tellg();
         } else {
             // Fallback: update based on downloadedSize if file check fails
             this->resumePosition += downloadedSize;
             std::cerr << "Warning: Could not determine file size after pause, using downloadedSize." << std::endl;
         }
         std::cout << "Download paused by callback at position: " << this->resumePosition << std::endl;
         return false; // Paused intentionally
    }

    // Check for other non-fatal conditions that might imply stopping but allow resume
    if (res == CURLE_OPERATION_TIMEDOUT || res == CURLE_PARTIAL_FILE) {
         // Update resume position similarly to pause
         std::ifstream checkFile(this->outputPath, std::ios::binary | std::ios::ate);
         if (checkFile.good()) {
             this->resumePosition = checkFile.tellg();
         } else {
             this->resumePosition += downloadedSize;
             std::cerr << "Warning: Could not determine file size after timeout/partial, using downloadedSize." << std::endl;
         }
         std::cout << "Download stopped (Timeout/Partial) at position: " << this->resumePosition << std::endl;
         // Decide if this should be treated as failure or just stopped state
         // For now, treat as failure for the 'downloadFinished' signal
         return false;
    }

    // Check for actual errors
    if (res != CURLE_OK) {
        std::cerr << "Download failed: " << curl_easy_strerror(res) << std::endl;
        if (errbuf[0] != '\0') {
            std::cerr << "Error details: " << errbuf << std::endl;
        }
        std::cerr << "HTTP response code: " << http_code << std::endl;
        // Don't reset resumePosition on failure, allow retrying from current point
        return false; // Failure
    }

    // If CURLE_OK and not paused/stopped above
    std::cout << "Download completed successfully! HTTP code: " << http_code << std::endl;
    this->resumePosition = 0; // Reset resume position only on full success
    return true; // Success
}

// Slot to start the download process
void Downloader::startDownload() {
    paused.store(false);
    resumePosition = 0; // Start from beginning
    totalFileSize = 0;  // Reset total file size

    // Optionally: Make a HEAD request to get total file size before downloading
    CURL* curlHead = curl_easy_init();
    if (curlHead) {
        curl_easy_setopt(curlHead, CURLOPT_URL, url.c_str()); // Use member url
        curl_easy_setopt(curlHead, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curlHead, CURLOPT_HEADER, 1L);
        curl_easy_setopt(curlHead, CURLOPT_FOLLOWLOCATION, 1L);

        // Add CAINFO for HEAD request as well for consistency
        QString appDir = QCoreApplication::applicationDirPath();
        QString caCertPath = QDir(appDir).filePath("certs/cacert.pem");
        // Correct type: Use std::string since toStdString() returns that
        std::string caCertPathStd = caCertPath.toStdString();
        QFileInfo caCertInfo(caCertPath);
        if (caCertInfo.exists() && caCertInfo.isFile()) {
            // Now caCertPathStd is std::string, so .c_str() is valid
            curl_easy_setopt(curlHead, CURLOPT_CAINFO, caCertPathStd.c_str());
            curl_easy_setopt(curlHead, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curlHead, CURLOPT_SSL_VERIFYHOST, 2L);
        } else {
             std::cerr << "Warning: CA cert bundle not found for HEAD request. Verification disabled for this request." << std::endl;
             curl_easy_setopt(curlHead, CURLOPT_SSL_VERIFYPEER, 0L);
             curl_easy_setopt(curlHead, CURLOPT_SSL_VERIFYHOST, 0L);
        }

        if (curl_easy_perform(curlHead) == CURLE_OK) {
            curl_easy_getinfo(curlHead, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &totalFileSize);
            std::cout << "Total file size from HEAD request: " << totalFileSize << std::endl;
        } else {
             std::cerr << "HEAD request failed: " << curl_easy_strerror(curl_easy_perform(curlHead)) << std::endl;
        }
        curl_easy_cleanup(curlHead);
    }

    // Call downloadFile without parameters
    bool result = downloadFile();
    emit downloadFinished(result);
}
// Slot to pause the download
// New method implementation
void Downloader::requestPause() {
    std::cout << "Pause requested directly" << std::endl;
    // Check if running to avoid emitting pause signal unnecessarily
    if (running.load()) {
         std::cout << "Setting paused flag to true directly" << std::endl;
         paused.store(true);
         // Emit the signal immediately from the calling thread (UI thread in this case)
         // This is safe because the connection in DownloadWindow is QueuedConnection
         emit downloadPaused();
    } else {
         std::cout << "Direct pause requested but download not running." << std::endl;
    }
}

// Existing pauseDownload slot (might be redundant now for button clicks)
void Downloader::pauseDownload() {
    std::cout << "Pause requested via slot" << std::endl; // Log difference
    if (running.load()) {
        std::cout << "Setting paused flag to true via slot" << std::endl;
        paused.store(true);
        emit downloadPaused(); // Signal emitted from downloader thread
    }
    else {
        std::cout << "Slot pause requested but download not running." << std::endl;
    }
}
// Slot to resume the download
void Downloader::resumeDownload() {
    std::cout << "Resume requested" << std::endl;
    if (!running.load() && paused.load()) {
        paused.store(false);

        // Update progress to show current status before resuming
        if (onProgress && totalFileSize > 0) { // Use member onProgress
            int currentPercent = 0;
            if (totalFileSize > 0) { // Avoid division by zero
                 currentPercent = static_cast<int>((static_cast<double>(resumePosition) * 100.0) / totalFileSize);
            }
            onProgress(std::min(100, std::max(0, currentPercent))); // Use member onProgress
        }

        emit downloadResumed();
        // Call downloadFile without parameters
        bool result = downloadFile();
        if (!paused.load()) {
            emit downloadFinished(result);
        }
    }
}

bool Downloader::isPaused() const {
    return paused.load();
}