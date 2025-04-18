#include <QApplication>
#include "downloadwindow.h"
#include <curl/curl.h>
#include <cstdlib>     // Required for atexit
#include <iostream>    // For potential error output

int main(int argc, char *argv[]) {

    CURLcode global_init_res = curl_global_init(CURL_GLOBAL_ALL);
    if (global_init_res != CURLE_OK) {
        // Handle error: Log it, maybe show a message box, and exit.
        std::cerr << "FATAL: curl_global_init() failed: "
                  << curl_easy_strerror(global_init_res) << std::endl;
        // Consider a simple QMessageBox if QApplication isn't ready yet
        // Or just return an error code.
        return 1;
    }

    int atexit_res = atexit(curl_global_cleanup);
    if (atexit_res != 0) {
         std::cerr << "WARNING: Failed to register curl_global_cleanup with atexit."
                   << std::endl;
    }

    QApplication app(argc, argv);
    DownloadWindow window;
    window.show();
    return app.exec();
}