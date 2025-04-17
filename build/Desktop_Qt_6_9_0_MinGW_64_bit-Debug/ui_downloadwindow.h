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
        progressBar->setGeometry(QRect(70, 140, 118, 23));
        progressBar->setValue(24);

        retranslateUi(DownloadWindow);

        QMetaObject::connectSlotsByName(DownloadWindow);
    } // setupUi

    void retranslateUi(QDialog *DownloadWindow)
    {
        DownloadWindow->setWindowTitle(QCoreApplication::translate("DownloadWindow", "Dialog", nullptr));
        downloadButton->setText(QCoreApplication::translate("DownloadWindow", "download", nullptr));
        label->setText(QCoreApplication::translate("DownloadWindow", "url", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DownloadWindow: public Ui_DownloadWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DOWNLOADWINDOW_H
