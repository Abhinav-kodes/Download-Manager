#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <string>

bool downloadFile(const std::string& url, const std::string& outputPath);

#endif // DOWNLOADER_H