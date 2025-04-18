QT += widgets

SOURCES += \
    src/main.cpp \
    src/downloader.cpp \
    src/downloadwindow.cpp

HEADERS += \
    include/downloader.h \
    include/downloadwindow.h \

MOC_DIR = build

FORMS += src/downloadwindow.ui

INCLUDEPATH += include

# Link libcurl
LIBS += -LE:/curl/lib -lcurl
INCLUDEPATH += E:/curl/include

CERT_DIR_SRC = $$PWD/certs
LIBCURL_DLL_SRC = $$PWD/libcurl-x64.dll
DEST_DEBUG = $$OUT_PWD/debug
DEST_RELEASE = $$OUT_PWD/release
certs_debug.files = $$CERT_DIR_SRC
certs_debug.path = $$DEST_DEBUG
certs_release.files = $$CERT_DIR_SRC
certs_release.path = $$DEST_RELEASE
dll_debug.files = $$LIBCURL_DLL_SRC
dll_debug.path = $$DEST_DEBUG
dll_release.files = $$LIBCURL_DLL_SRC
dll_release.path = $$DEST_RELEASE

COPIES += certs_debug certs_release dll_debug dll_release
