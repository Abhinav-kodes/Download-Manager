#include "downloader.h"
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <QCoreApplication>
#include <functional>

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
      // mutex default initialized
      totalFileSize(0),
      m_curlHandle(nullptr)
{
    // Constructor body
}

// downloadFile function updated to use CurlCallbackContext
bool Downloader::downloadFile(const std::string& url, const std::string& outputPath, std::function<void(int)> onProgress) {

    CURL* curl; // Use local variable for the handle within this function scope
    CURLcode res;

    // Open file in appropriate mode based on resume position
    std::ios_base::openmode mode = std::ios::binary;
    if (this->resumePosition > 0) { // Use member variable
        mode |= std::ios::in | std::ios::out; // Open for update
    } else {
        mode |= std::ios::out | std::ios::trunc; // Create/Truncate new file (changed from just ios::out)
    }

    // Use member variable for path
    std::ofstream file(this->outputPath, mode);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << this->outputPath << std::endl;
        return false;
    }

    // Seek to resume position if needed
    if (this->resumePosition > 0) { // Use member variable
        file.seekp(this->resumePosition);
        std::cout << "Resuming download from position: " << this->resumePosition << std::endl;
    }

    // NOTE: Getting total file size here might be better handled elsewhere or using HEAD request result
    if (this->resumePosition > 0 && this->totalFileSize == 0) { // Use member variables
        std::ifstream checkFile(this->outputPath, std::ios::binary | std::ios::ate);
        if (checkFile.good()) {
            this->totalFileSize = std::max(this->totalFileSize, (curl_off_t)checkFile.tellg());
        }
    }

    curl = curl_easy_init(); // Initialize local handle

    this->m_curlHandle = curl; // Assign to member variable

    if (!curl) {
        std::cerr << "Failed to initialize cURL." << std::endl;
        file.close();
        // NOTE: curl_global_cleanup should ideally match init call (e.g., on application exit).
        // curl_global_cleanup(); // Assuming called elsewhere
        this->m_curlHandle = nullptr; // Ensure member is null on failure
        return false;
    }

    // --- Set Curl Options using Context Struct ---

    // Use member variable for URL (or the parameter if kept)
    curl_easy_setopt(curl, CURLOPT_URL, this->url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    // ++ NEW: Create and populate the context struct ++
    CurlCallbackContext callbackContext;
    callbackContext.fileStream = &file;         // Address of local file stream
    callbackContext.pausedFlag = &this->paused; // Address of member atomic bool
    // Use member variable for onProgress (or parameter if kept)
    callbackContext.progressFn = &this->onProgress;
    callbackContext.resumeOffset = this->resumePosition; // Copy member resume position

    // ++ NEW: Pass the context struct to both callbacks ++
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &callbackContext);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &callbackContext); // Use same context

    // Set resume position if needed (using value from context now)
    if (callbackContext.resumeOffset > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, callbackContext.resumeOffset);
    }

    // Set progress function options
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    // CURLOPT_XFERINFODATA already set above

    // --- Set other options ---
    // NOTE: Consider enabling SSL verification (1L) and providing CA info
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // SECURITY RISK!
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Optional: for debugging
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"); // Or your app name
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 8192L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 3L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 0L); // Consider removing if not needed

    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    errbuf[0] = 0;

    // --- Perform Download ---
    this->running.store(true); // Mark as running

    res = curl_easy_perform(curl); // BLOCKING call

    this->running.store(false); // Mark as not running anymore

    // --- Process Results ---
    curl_off_t downloadedSize = 0;
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &downloadedSize);

    if (downloadedSize > 0) {
        this->resumePosition += downloadedSize; // Update member variable
        std::cout << "Downloaded size in this transfer: " << downloadedSize << ", new resume position: " << this->resumePosition << std::endl;
    }

    // Update total file size after download
    curl_off_t dlTotal = 0;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &dlTotal);
    if (dlTotal > 0) {
        this->totalFileSize = this->resumePosition + dlTotal; // Update member variable
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // --- Cleanup ---
    this->m_curlHandle = nullptr; // Clear member handle pointer
    curl_easy_cleanup(curl); // Clean up the local easy handle
    file.close();

    // --- Determine Return Status ---
    // Check if paused based on flag AND specific return codes from callbacks
    if (this->paused.load() &&
        (res == CURLE_WRITE_ERROR || res == CURLE_ABORTED_BY_CALLBACK)) {
         std::cout << "Download paused by callback at position: " << this->resumePosition << std::endl;
         return false; // Paused intentionally
    }
    // Check for other non-fatal conditions that might imply pausing or stopping
    if (res == CURLE_OPERATION_TIMEDOUT || res == CURLE_PARTIAL_FILE) {
         std::cout << "Download stopped (Timeout/Partial) at position: " << this->resumePosition << std::endl;
    }

    // Check for actual errors
    if (res != CURLE_OK) {
        std::cerr << "Download failed: " << curl_easy_strerror(res) << std::endl;
        if (errbuf[0] != '\0') {
            std::cerr << "Error details: " << errbuf << std::endl;
        }
        std::cerr << "HTTP response code: " << http_code << std::endl;
        return false; // Failure
    }

    // If CURLE_OK and not paused
    std::cout << "Download completed successfully! HTTP code: " << http_code << std::endl;
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
        emit downloadPaused();
    } 
    else {
        std::cout << "Pause requested but download not running." << std::endl;
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