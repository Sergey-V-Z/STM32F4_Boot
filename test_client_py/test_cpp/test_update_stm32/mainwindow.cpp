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
    ui->m_backupUpdateCheckBox->setToolTip("Обновить резервную прошивку вместо основной");

    // Настраиваем соединения сигналов и слотов
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->statusButton, &QPushButton::clicked, this, &MainWindow::onStatusClicked);
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::onSelectFirmwareClicked);
    connect(ui->updateButton, &QPushButton::clicked, this, &MainWindow::onUpdateClicked);
    connect(ui->abortButton, &QPushButton::clicked, this, &MainWindow::onAbortClicked);
    // соединение для метаданных
    connect(m_updater, &FirmwareUpdater::metadataReceived, this, &MainWindow::onMetadataReceived);
    // Если путь к файлу меняется через QLineEdit
    connect(ui->firmwarePathEdit, &QLineEdit::textChanged, this, &MainWindow::onFirmwareSelected);
    connect(m_updater, &FirmwareUpdater::metadataMismatch, this, &MainWindow::onMetadataMismatch);

    // Соединения с FirmwareUpdater
    connect(m_updater, &FirmwareUpdater::updateProgress, this, &MainWindow::onUpdateProgress);
    connect(m_updater, &FirmwareUpdater::updateStatusChanged, this, &MainWindow::onUpdateStatusChanged);
    connect(m_updater, &FirmwareUpdater::logMessage, this, &MainWindow::onLogMessage);

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

            // Запрашиваем метаданные при успешном подключении
            FirmwareMetadata metadata;
            m_updater->getDeviceMetadata(metadata);
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

    // Проверяем наличие метаданных в файле перед обновлением
    FirmwareMetadata fileMetadata = m_updater->extractMetadataFromFile(firmwarePath);
    if (fileMetadata.key_start != METADATA_KEY) {
        QString errorMessage = "В файле не найдены валидные метаданные.\n"
                               "Обновление невозможно. Используйте только официальные прошивки.";
        QMessageBox::critical(this, "Ошибка метаданных", errorMessage);
        ui->logTextEdit->appendPlainText("ОШИБКА: В файле не найдены валидные метаданные. Обновление отменено.");
        return; // Отменяем обновление если нет метаданных
    }

    // Проверяем состояние чекбокса из UI
    bool isBackup = ui->m_backupUpdateCheckBox->isChecked();

    // Показываем подтверждение с указанием типа обновления
    QString updateType = isBackup ? "резервную" : "основную";
    QString message = QString("Вы уверены, что хотите обновить %1 прошивку?").arg(updateType);
    if (isBackup) {
        message += "\nЭто действие не повлияет на текущую работу устройства.";
    }

    if (QMessageBox::question(this, "Подтверждение", message,
                            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    // Блокируем интерфейс на время обновления
    setControlsEnabled(false);
    ui->connectButton->setEnabled(false);
    ui->abortButton->setEnabled(true);

    // Запускаем обновление
    // Запускаем обновление (передаем флаг isBackup)
    QString logMessage = QString("Начинаем обновление %1 прошивки...").arg(updateType);
    ui->logTextEdit->appendPlainText(logMessage);
    bool success = m_updater->updateFirmware(firmwarePath, isBackup);

    if (!success) {
        // Метаданные могут не совпасть - это обрабатывается через сигнал
        // Если ошибка не связана с метаданными - покажем сообщение
        if (fileMetadata.key_start == METADATA_KEY) {
            // Разблокируем интерфейс только если это не ошибка метаданных
            // (для ошибки метаданных это сделает обработчик onMetadataMismatch)
            setControlsEnabled(true);
            ui->connectButton->setEnabled(true);
            ui->abortButton->setEnabled(false);
        }
    } else {
        QString successMessage = QString("Обновление %1 прошивки успешно завершено").arg(updateType);
        QMessageBox::information(this, "Успех", successMessage);

        setControlsEnabled(true);
        ui->connectButton->setEnabled(true);
        ui->abortButton->setEnabled(false);
    }
    /*
    if (success) {
        QString successMessage = QString("Обновление %1 прошивки успешно завершено").arg(updateType);
        QMessageBox::information(this, "Успех", successMessage);
    } else {
        QString errorMessage = QString("Не удалось обновить %1 прошивку").arg(updateType);
        QMessageBox::critical(this, "Ошибка", errorMessage);
    }*/

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

// Реализация слота для получения метаданных устройства
void MainWindow::onMetadataReceived(const FirmwareMetadata &metadata)
{
    if (metadata.key_start == METADATA_KEY) {
        QString deviceInfo = QString("Подключено: %1 (версия %2)")
            .arg(QString::fromUtf8(metadata.name_proj))
            .arg(QString::number(metadata.version, 16).toUpper().rightJustified(8, '0'));

        // Показываем в статусной строке или в label
        ui->deviceInfoLabel->setText(deviceInfo);

        // Логируем
        ui->logTextEdit->appendPlainText("Информация об устройстве: " + deviceInfo);
    }
}

// Реализация слота для выбора файла
void MainWindow::onFirmwareSelected(const QString &firmwarePath)
{
    if (!firmwarePath.isEmpty() && QFile::exists(firmwarePath)) {
        // Извлекаем метаданные из файла
        FirmwareMetadata fileMetadata = m_updater->extractMetadataFromFile(firmwarePath);

        if (fileMetadata.key_start == METADATA_KEY) {
            QString fileInfo = QString("Файл: %1 (версия %2)")
                .arg(QString::fromUtf8(fileMetadata.name_proj))
                .arg(QString::number(fileMetadata.version, 16).toUpper().rightJustified(8, '0'));

            // Показываем в UI (например, в label)
            ui->fileInfoLabel->setText(fileInfo);
        } else {
            ui->fileInfoLabel->setText("Метаданные не найдены в файле");
        }
    }
}

// Обработчик несоответствия метаданных
void MainWindow::onMetadataMismatch(const QString &fileProject, const QString &deviceProject)
{
    // Выводим сообщение в лог
    ui->logTextEdit->appendPlainText("ПРЕДУПРЕЖДЕНИЕ: Несоответствие метаданных!");
    ui->logTextEdit->appendPlainText("Файл: " + fileProject);
    ui->logTextEdit->appendPlainText("Устройство: " + deviceProject);
    ui->logTextEdit->appendPlainText("Обновление отменено из-за несоответствия метаданных.");

}
