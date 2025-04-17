#include "downloader.h"
#include <curl/curl.h>
#include <fstream>
#include <iostream>

// WriteCallback function to write data to file
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::ofstream* out = static_cast<std::ofstream*>(userp);
    out->write(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// Progress callback function to update the download progress
int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* progressFunc = static_cast<std::function<void(int)>*>(clientp);
    if (progressFunc && dltotal > 0) {
        int percent = static_cast<int>((dlnow * 100) / dltotal);
        (*progressFunc)(percent); // Update progress with the current percentage
    }
    return 0;
}

// Constructor for the Downloader class
Downloader::Downloader(const std::string& url, const std::string& outputPath, std::function<void(int)> onProgress)
    : url(url), outputPath(outputPath), onProgress(onProgress) {}

// Function to download the file
bool Downloader::downloadFile(const std::string& url, const std::string& outputPath, std::function<void(int)> onProgress) {
    CURL* curl;
    CURLcode res;
    std::ofstream file(outputPath, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << outputPath << std::endl;
        return false;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize cURL." << std::endl;
        file.close();
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

    if (onProgress) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &onProgress);
    }

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Temporarily disable SSL verification
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Enable verbose output for debugging
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    errbuf[0] = 0; // Empty string

    res = curl_easy_perform(curl);


    long http_code = 0;

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);


    curl_easy_cleanup(curl);

    curl_global_cleanup();

    file.close();


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
    bool result = downloadFile(url, outputPath, onProgress);
    emit downloadFinished(result);
} 