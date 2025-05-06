/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.14.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QLineEdit *ipAddressEdit;
    QSpinBox *portSpinBox;
    QPushButton *connectButton;
    QPushButton *statusButton;
    QLineEdit *firmwarePathEdit;
    QPushButton *browseButton;
    QSpinBox *blockSizeSpinBox;
    QLineEdit *versionEdit;
    QPushButton *updateButton;
    QPushButton *abortButton;
    QProgressBar *progressBar;
    QLabel *statusLabel;
    QPlainTextEdit *logTextEdit;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(800, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        ipAddressEdit = new QLineEdit(centralwidget);
        ipAddressEdit->setObjectName(QString::fromUtf8("ipAddressEdit"));
        ipAddressEdit->setGeometry(QRect(50, 40, 181, 20));
        portSpinBox = new QSpinBox(centralwidget);
        portSpinBox->setObjectName(QString::fromUtf8("portSpinBox"));
        portSpinBox->setGeometry(QRect(260, 40, 101, 22));
        portSpinBox->setMaximum(10000);
        portSpinBox->setValue(8080);
        connectButton = new QPushButton(centralwidget);
        connectButton->setObjectName(QString::fromUtf8("connectButton"));
        connectButton->setGeometry(QRect(390, 30, 75, 23));
        statusButton = new QPushButton(centralwidget);
        statusButton->setObjectName(QString::fromUtf8("statusButton"));
        statusButton->setGeometry(QRect(50, 110, 75, 23));
        firmwarePathEdit = new QLineEdit(centralwidget);
        firmwarePathEdit->setObjectName(QString::fromUtf8("firmwarePathEdit"));
        firmwarePathEdit->setGeometry(QRect(50, 170, 281, 20));
        firmwarePathEdit->setReadOnly(true);
        browseButton = new QPushButton(centralwidget);
        browseButton->setObjectName(QString::fromUtf8("browseButton"));
        browseButton->setGeometry(QRect(360, 170, 75, 23));
        blockSizeSpinBox = new QSpinBox(centralwidget);
        blockSizeSpinBox->setObjectName(QString::fromUtf8("blockSizeSpinBox"));
        blockSizeSpinBox->setGeometry(QRect(50, 240, 101, 22));
        blockSizeSpinBox->setMaximum(10000);
        blockSizeSpinBox->setValue(256);
        versionEdit = new QLineEdit(centralwidget);
        versionEdit->setObjectName(QString::fromUtf8("versionEdit"));
        versionEdit->setGeometry(QRect(200, 240, 113, 20));
        updateButton = new QPushButton(centralwidget);
        updateButton->setObjectName(QString::fromUtf8("updateButton"));
        updateButton->setGeometry(QRect(370, 240, 75, 23));
        abortButton = new QPushButton(centralwidget);
        abortButton->setObjectName(QString::fromUtf8("abortButton"));
        abortButton->setGeometry(QRect(370, 280, 75, 23));
        progressBar = new QProgressBar(centralwidget);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        progressBar->setGeometry(QRect(60, 360, 118, 23));
        progressBar->setValue(24);
        statusLabel = new QLabel(centralwidget);
        statusLabel->setObjectName(QString::fromUtf8("statusLabel"));
        statusLabel->setGeometry(QRect(50, 410, 431, 16));
        logTextEdit = new QPlainTextEdit(centralwidget);
        logTextEdit->setObjectName(QString::fromUtf8("logTextEdit"));
        logTextEdit->setGeometry(QRect(500, 20, 261, 541));
        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 800, 22));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        ipAddressEdit->setText(QCoreApplication::translate("MainWindow", "192.168.10.3", nullptr));
        connectButton->setText(QCoreApplication::translate("MainWindow", "PushButton", nullptr));
        statusButton->setText(QCoreApplication::translate("MainWindow", "\321\201\321\202\320\260\321\202\321\203\321\201", nullptr));
        browseButton->setText(QCoreApplication::translate("MainWindow", "PushButton", nullptr));
        versionEdit->setText(QCoreApplication::translate("MainWindow", "10000", nullptr));
        updateButton->setText(QCoreApplication::translate("MainWindow", "PushButton", nullptr));
        abortButton->setText(QCoreApplication::translate("MainWindow", "\320\276\321\202\320\274\320\265\320\275\320\260", nullptr));
        statusLabel->setText(QCoreApplication::translate("MainWindow", "TextLabel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
