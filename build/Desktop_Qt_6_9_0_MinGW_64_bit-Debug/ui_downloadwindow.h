/********************************************************************************
** Form generated from reading UI file 'downloadwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DOWNLOADWINDOW_H
#define UI_DOWNLOADWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_DownloadWindow
{
public:
    QLineEdit *urlLineEdit;
    QPushButton *downloadButton;
    QLabel *label;
    QProgressBar *progressBar;
    QPushButton *pauseResumeButton;
    QLabel *sizeLabel;
    QLabel *speedLabel;

    void setupUi(QDialog *DownloadWindow)
    {
        if (DownloadWindow->objectName().isEmpty())
            DownloadWindow->setObjectName("DownloadWindow");
        DownloadWindow->resize(400, 300);
        urlLineEdit = new QLineEdit(DownloadWindow);
        urlLineEdit->setObjectName("urlLineEdit");
        urlLineEdit->setGeometry(QRect(50, 40, 321, 28));
        downloadButton = new QPushButton(DownloadWindow);
        downloadButton->setObjectName("downloadButton");
        downloadButton->setGeometry(QRect(280, 90, 83, 29));
        label = new QLabel(DownloadWindow);
        label->setObjectName("label");
        label->setGeometry(QRect(80, 10, 63, 20));
        progressBar = new QProgressBar(DownloadWindow);
        progressBar->setObjectName("progressBar");
        progressBar->setGeometry(QRect(47, 140, 201, 31));
        progressBar->setValue(0);
        pauseResumeButton = new QPushButton(DownloadWindow);
        pauseResumeButton->setObjectName("pauseResumeButton");
        pauseResumeButton->setGeometry(QRect(280, 140, 83, 29));
        sizeLabel = new QLabel(DownloadWindow);
        sizeLabel->setObjectName("sizeLabel");
        sizeLabel->setGeometry(QRect(130, 90, 63, 20));
        speedLabel = new QLabel(DownloadWindow);
        speedLabel->setObjectName("speedLabel");
        speedLabel->setGeometry(QRect(120, 190, 101, 31));

        retranslateUi(DownloadWindow);

        QMetaObject::connectSlotsByName(DownloadWindow);
    } // setupUi

    void retranslateUi(QDialog *DownloadWindow)
    {
        DownloadWindow->setWindowTitle(QCoreApplication::translate("DownloadWindow", "Dialog", nullptr));
        downloadButton->setText(QCoreApplication::translate("DownloadWindow", "download", nullptr));
        label->setText(QCoreApplication::translate("DownloadWindow", "url", nullptr));
        pauseResumeButton->setText(QCoreApplication::translate("DownloadWindow", "Pause", nullptr));
        sizeLabel->setText(QCoreApplication::translate("DownloadWindow", "Size", nullptr));
        speedLabel->setText(QCoreApplication::translate("DownloadWindow", "Speed", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DownloadWindow: public Ui_DownloadWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DOWNLOADWINDOW_H
