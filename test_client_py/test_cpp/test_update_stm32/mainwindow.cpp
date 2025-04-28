#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_updater(new FirmwareUpdater(this))
{
    ui->setupUi(this);

    // Настраиваем соединения сигналов и слотов
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->statusButton, &QPushButton::clicked, this, &MainWindow::onStatusClicked);
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::onSelectFirmwareClicked);
    connect(ui->updateButton, &QPushButton::clicked, this, &MainWindow::onUpdateClicked);
    connect(ui->abortButton, &QPushButton::clicked, this, &MainWindow::onAbortClicked);

    // Соединения с FirmwareUpdater
    connect(m_updater, &FirmwareUpdater::updateProgress,
            this, &MainWindow::onUpdateProgress);
    connect(m_updater, &FirmwareUpdater::updateStatusChanged,
            this, &MainWindow::onUpdateStatusChanged);
    connect(m_updater, &FirmwareUpdater::logMessage,
            this, &MainWindow::onLogMessage);

    // Настройка начального состояния UI
    ui->statusButton->setEnabled(false);
    ui->updateButton->setEnabled(false);
    ui->abortButton->setEnabled(false);
    ui->progressBar->setValue(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onConnectClicked()
{
    if (m_updater->isConnected()) {
        // Отключаемся если уже подключены
        m_updater->disconnectFromDevice();
        ui->connectButton->setText("Подключить");
        ui->statusButton->setEnabled(false);
        ui->updateButton->setEnabled(false);
        ui->abortButton->setEnabled(false);
        ui->statusLabel->setText("Статус: Не подключен");
    } else {
        // Подключаемся к устройству
        QString ipAddress = ui->ipAddressEdit->text();
        int port = ui->portSpinBox->value();

        ui->logTextEdit->appendPlainText("Подключение к " + ipAddress + ":" + QString::number(port) + "...");

        if (m_updater->connectToDevice(ipAddress, port)) {
            ui->connectButton->setText("Отключить");
            ui->statusButton->setEnabled(true);
            ui->updateButton->setEnabled(!ui->firmwarePathEdit->text().isEmpty());
            ui->abortButton->setEnabled(true);

            // Запрашиваем статус устройства
            quint32 status;
            quint32 error;
            m_updater->getDeviceStatus(status, error);
            updateStatusDisplay(status, error);
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось подключиться к устройству");
        }
    }
}

void MainWindow::onStatusClicked()
{
    quint32 status;
    quint32 error;
    if (m_updater->getDeviceStatus(status, error)) {
        updateStatusDisplay(status, error);
    }
}

void MainWindow::onSelectFirmwareClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Выбрать файл прошивки", "", "Все файлы (*.*)");
    if (!fileName.isEmpty()) {
        ui->firmwarePathEdit->setText(fileName);
        ui->updateButton->setEnabled(m_updater->isConnected());
    }
}

void MainWindow::onUpdateClicked()
{
    QString firmwarePath = ui->firmwarePathEdit->text();
    if (firmwarePath.isEmpty()) {
        QMessageBox::warning(this, "Предупреждение", "Выберите файл прошивки");
        return;
    }

    // Блокируем интерфейс на время обновления
    setControlsEnabled(false);
    ui->connectButton->setEnabled(false);
    ui->abortButton->setEnabled(true);

    // Получаем параметры обновления
    quint32 blockSize = ui->blockSizeSpinBox->value();
    quint32 firmwareVersion = ui->versionEdit->text().toUInt(nullptr, 16);

    // Запускаем обновление
    ui->logTextEdit->appendPlainText("Начинаем обновление прошивки...");
    bool success = m_updater->updateFirmware(firmwarePath, blockSize, firmwareVersion);

    if (success) {
        QMessageBox::information(this, "Успех", "Обновление прошивки успешно завершено");
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось обновить прошивку");
    }

    setControlsEnabled(true);
    ui->connectButton->setEnabled(true);
}

void MainWindow::onAbortClicked()
{
    if (QMessageBox::question(this, "Подтверждение",
                            "Вы уверены, что хотите отменить обновление?",
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_updater->abortUpdate();
        setControlsEnabled(true);
        ui->connectButton->setEnabled(true);
    }
}

void MainWindow::onUpdateProgress(int percent, quint32 currentBlock, quint32 totalBlocks)
{
    ui->progressBar->setValue(percent);
    ui->statusLabel->setText(QString("Блок %1/%2 (%3%)").arg(currentBlock).arg(totalBlocks).arg(percent));
}

void MainWindow::onUpdateStatusChanged(quint8 status, quint32 error)
{
    updateStatusDisplay(status, error);

    // Если обновление завершено или произошла ошибка
    if (status == UPDATE_STATUS_COMPLETE || status == UPDATE_STATUS_ERROR) {
        setControlsEnabled(true);
        ui->connectButton->setEnabled(true);
    }
}

void MainWindow::onLogMessage(const QString &message)
{
    ui->logTextEdit->appendPlainText(message);
}

void MainWindow::updateStatusDisplay(quint8 status, quint32 error)
{
    QString statusText = getStatusText(status);

    if (error != UPDATE_ERROR_NONE) {
        statusText += " - " + getErrorText(error);
    }

    ui->statusLabel->setText("Статус: " + statusText);
}

void MainWindow::setControlsEnabled(bool enabled)
{
    ui->ipAddressEdit->setEnabled(enabled);
    ui->portSpinBox->setEnabled(enabled);
    ui->firmwarePathEdit->setEnabled(enabled);
    ui->blockSizeSpinBox->setEnabled(enabled);
    ui->versionEdit->setEnabled(enabled);
    ui->browseButton->setEnabled(enabled);
}

QString MainWindow::getStatusText(quint8 status)
{
    switch (status) {
        case UPDATE_STATUS_IDLE:
            return "Ожидание";
        case UPDATE_STATUS_IN_PROGRESS:
            return "В процессе обновления";
        case UPDATE_STATUS_COMPLETE:
            return "Обновление завершено";
        case UPDATE_STATUS_ERROR:
            return "Ошибка";
        case UPDATE_STATUS_READY_REBOOT:
            return "Готов к перезагрузке";
        default:
            return QString("Неизвестный (%1)").arg(status);
    }
}

QString MainWindow::getErrorText(quint32 error)
{
    switch (error) {
        case UPDATE_ERROR_NONE:
            return "Нет ошибок";
        case UPDATE_ERROR_INVALID_CMD:
            return "Неверная команда";
        case UPDATE_ERROR_INVALID_SIZE:
            return "Неверный размер";
        case UPDATE_ERROR_FLASH_FAILURE:
            return "Ошибка записи во флеш";
        case UPDATE_ERROR_CRC_MISMATCH:
            return "Ошибка CRC";
        case UPDATE_ERROR_SEQ_ERROR:
            return "Ошибка последовательности";
        case UPDATE_ERROR_BUSY:
            return "Устройство занято";
        case UPDATE_ERROR_ABORT:
            return "Обновление отменено";
        default:
            return QString("Неизвестная ошибка (%1)").arg(error);
    }
}
