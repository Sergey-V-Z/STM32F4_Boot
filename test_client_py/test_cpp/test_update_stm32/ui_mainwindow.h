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
#include <QtWidgets/QCheckBox>
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
    QPushButton *updateButton;
    QPushButton *abortButton;
    QProgressBar *progressBar;
    QLabel *statusLabel;
    QPlainTextEdit *logTextEdit;
    QCheckBox *m_backupUpdateCheckBox;
    QLabel *deviceInfoLabel;
    QLabel *fileInfoLabel;
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
        connectButton->setGeometry(QRect(380, 40, 91, 23));
        statusButton = new QPushButton(centralwidget);
        statusButton->setObjectName(QString::fromUtf8("statusButton"));
        statusButton->setGeometry(QRect(30, 270, 75, 23));
        firmwarePathEdit = new QLineEdit(centralwidget);
        firmwarePathEdit->setObjectName(QString::fromUtf8("firmwarePathEdit"));
        firmwarePathEdit->setGeometry(QRect(30, 110, 431, 20));
        firmwarePathEdit->setReadOnly(true);
        browseButton = new QPushButton(centralwidget);
        browseButton->setObjectName(QString::fromUtf8("browseButton"));
        browseButton->setGeometry(QRect(240, 140, 111, 23));
        updateButton = new QPushButton(centralwidget);
        updateButton->setObjectName(QString::fromUtf8("updateButton"));
        updateButton->setGeometry(QRect(360, 170, 121, 23));
        abortButton = new QPushButton(centralwidget);
        abortButton->setObjectName(QString::fromUtf8("abortButton"));
        abortButton->setGeometry(QRect(220, 180, 121, 23));
        progressBar = new QProgressBar(centralwidget);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        progressBar->setGeometry(QRect(40, 190, 118, 23));
        progressBar->setValue(24);
        statusLabel = new QLabel(centralwidget);
        statusLabel->setObjectName(QString::fromUtf8("statusLabel"));
        statusLabel->setGeometry(QRect(30, 240, 431, 16));
        logTextEdit = new QPlainTextEdit(centralwidget);
        logTextEdit->setObjectName(QString::fromUtf8("logTextEdit"));
        logTextEdit->setGeometry(QRect(500, 20, 261, 541));
        m_backupUpdateCheckBox = new QCheckBox(centralwidget);
        m_backupUpdateCheckBox->setObjectName(QString::fromUtf8("m_backupUpdateCheckBox"));
        m_backupUpdateCheckBox->setGeometry(QRect(370, 140, 101, 18));
        deviceInfoLabel = new QLabel(centralwidget);
        deviceInfoLabel->setObjectName(QString::fromUtf8("deviceInfoLabel"));
        deviceInfoLabel->setGeometry(QRect(10, 310, 431, 16));
        fileInfoLabel = new QLabel(centralwidget);
        fileInfoLabel->setObjectName(QString::fromUtf8("fileInfoLabel"));
        fileInfoLabel->setGeometry(QRect(10, 350, 451, 16));
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
        connectButton->setText(QCoreApplication::translate("MainWindow", "\320\237\320\276\320\264\320\272\320\273\321\216\321\207\320\265\320\275\320\270\320\265", nullptr));
        statusButton->setText(QCoreApplication::translate("MainWindow", "\321\201\321\202\320\260\321\202\321\203\321\201", nullptr));
        browseButton->setText(QCoreApplication::translate("MainWindow", "\320\244\320\260\320\271\320\273 \320\277\321\200\320\276\321\210\320\270\320\262\320\272\320\270", nullptr));
        updateButton->setText(QCoreApplication::translate("MainWindow", "\320\235\320\260\321\207\320\260\321\202\321\214 \320\276\320\261\320\275\320\276\320\262\320\273\320\265\320\275\320\270\320\265", nullptr));
        abortButton->setText(QCoreApplication::translate("MainWindow", "\320\276\321\202\320\274\320\265\320\275\320\260", nullptr));
        statusLabel->setText(QCoreApplication::translate("MainWindow", "TextLabel", nullptr));
        m_backupUpdateCheckBox->setText(QCoreApplication::translate("MainWindow", "\321\200\320\265\320\267\320\265\321\200\320\262\320\275\320\260\321\217", nullptr));
        deviceInfoLabel->setText(QCoreApplication::translate("MainWindow", "TextLabel", nullptr));
        fileInfoLabel->setText(QCoreApplication::translate("MainWindow", "123", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
