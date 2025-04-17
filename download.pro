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

CONFIG += file_copies
COPIES += dllcopy
dllcopy.files = $$PWD/libcurl-x64.dll
dllcopy.path = $$OUT_PWD/debug  # also add /release if needed 