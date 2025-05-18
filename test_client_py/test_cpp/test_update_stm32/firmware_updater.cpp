/**
 * @file firmware_updater.cpp
 * @brief Реализация клиента обновления прошивки
 */

#include "firmware_updater.h"
#include <QDebug>
#include <QDataStream>
#include <QCryptographicHash>
#include <QThread>
#include <QCoreApplication>

FirmwareUpdater::FirmwareUpdater(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_timeoutTimer(new QTimer(this))
    , m_isUpdating(false)
    , m_retryCount(DEFAULT_RETRY_COUNT)
    , m_retryDelay(DEFAULT_DELAY)
    , m_blockSize(DEFAULT_BLOCK_SIZE)
    , m_currentBlock(0)
    , m_totalBlocks(0)
    , m_firmwareCrc(0)
{
    // Настраиваем таймер ожидания
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(DEFAULT_TIMEOUT);
    
    // Подключаем сигналы сокета
    connect(m_socket, &QTcpSocket::connected, this, &FirmwareUpdater::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &FirmwareUpdater::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &FirmwareUpdater::onSocketReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &FirmwareUpdater::onSocketError);
    
    // Подключаем сигнал таймера
    connect(m_timeoutTimer, &QTimer::timeout, this, &FirmwareUpdater::onTimeout);
    
    log("Клиент обновления прошивки инициализирован");
}

FirmwareUpdater::~FirmwareUpdater()
{
    disconnectFromDevice();
    
    delete m_socket;
    delete m_timeoutTimer;
}

bool FirmwareUpdater::connectToDevice(const QString &hostAddress, quint16 port)
{
    // Сначала отключаемся, если уже подключены
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        disconnectFromDevice();
    }
    
    log(QString("Подключение к %1:%2...").arg(hostAddress).arg(port));
    
    // Включаем опцию TCP_NODELAY для отключения алгоритма Нагла
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    
    // Устанавливаем соединение
    m_socket->connectToHost(hostAddress, port);
    
    // Ждём установления соединения с таймаутом
    m_timeoutTimer->start();
    bool connected = m_socket->waitForConnected(DEFAULT_TIMEOUT);
    m_timeoutTimer->stop();
    
    if (!connected) {
        log(QString("Ошибка подключения: %1").arg(m_socket->errorString()));
        return false;
    }
    
    log("Подключение установлено");
    return true;
}

void FirmwareUpdater::disconnectFromDevice()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        log("Отключение от устройства...");
        m_socket->disconnectFromHost();
        
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }
    
    resetState();
}

bool FirmwareUpdater::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

bool FirmwareUpdater::checkConnection()
{
    log("Проверка связи с устройством...");
    
    for (int retry = 0; retry < m_retryCount; retry++) {
        if (sendPacket(CMD_PING, 0, 0)) {
            quint32 status;
            quint32 error;
            
            if (receiveResponse(status, error)) {
                log(QString("Устройство отвечает, статус: %1").arg(getStatusText(status)));
                return true;
            }
        }
        
        log(QString("Попытка %1/%2 не удалась, повторяем...").arg(retry + 1).arg(m_retryCount));
        QThread::msleep(m_retryDelay);
    }
    
    log("Ошибка: устройство не отвечает на команду PING");
    return false;
}

bool FirmwareUpdater::getDeviceStatus(quint32 &status, quint32 &error)
{
    if (sendPacket(CMD_GET_STATUS, 0, 0)) {
        if (receiveResponse(status, error)) {
            log(QString("Статус устройства: %1").arg(getStatusText(status)));
            
            if (error != UPDATE_ERROR_NONE) {
                log(QString("Код ошибки: %1").arg(getErrorText(error)));
            }
            
            emit deviceStatusReceived(status, error);
            return true;
        }
    }
    
    log("Ошибка: не удалось получить статус устройства");
    return false;
}

bool FirmwareUpdater::abortUpdate()
{
    log("Отмена процесса обновления...");
    
    if (sendPacket(CMD_ABORT_UPDATE, 0, 0)) {
        quint32 status;
        quint32 error;
        
        if (receiveResponse(status, error)) {
            if (status == UPDATE_STATUS_IDLE) {
                log("Процесс обновления успешно отменен");
                return true;
            } else {
                log(QString("Ошибка при отмене обновления: %1").arg(getErrorText(error)));
            }
        }
    }
    
    log("Ошибка: не удалось отменить процесс обновления");
    return false;
}

bool FirmwareUpdater::updateFirmware(const QString &firmwarePath, bool isBackup)
{
    // извлечение и проверку метаданных
    FirmwareMetadata fileMetadata = extractMetadataFromFile(firmwarePath);

    // Проверка наличия метаданных в файле
    if (fileMetadata.key_start != METADATA_KEY) {
        log("ОШИБКА: В файле не найдены валидные метаданные");
        log("Обновление прервано. Используйте только прошивки с корректными метаданными.");
        return false; // Отменяем обновление если нет метаданных
    }

    // Получаем метаданные с устройства для сравнения
    FirmwareMetadata deviceMetadata;
    if (getDeviceMetadata(deviceMetadata)) {
        // Сравниваем имена проектов
        if (fileMetadata.key_start == METADATA_KEY && deviceMetadata.key_start == METADATA_KEY) {
            QString fileProjName = QString::fromUtf8(fileMetadata.name_proj);
            QString deviceProjName = QString::fromUtf8(deviceMetadata.name_proj);

            if (fileProjName != deviceProjName) {
                log(QString("ПРЕДУПРЕЖДЕНИЕ: Несоответствие имён проектов!"));
                log(QString("Файл: '%1'").arg(fileProjName));
                log(QString("Устройство: '%1'").arg(deviceProjName));

                // Можно добавить диалог подтверждения
                // return false; // или диалог пользователю
            }
        }
    }

    // Открываем файл прошивки
    QFile firmwareFile(firmwarePath);
    if (!firmwareFile.open(QIODevice::ReadOnly)) {
        log(QString("Ошибка при открытии файла прошивки: %1").arg(firmwareFile.errorString()));
        return false;
    }
    
    // Считываем данные прошивки
    QByteArray firmwareData = firmwareFile.readAll();
    firmwareFile.close();
    
    // Получаем размер и CRC
    quint32 firmwareSize = firmwareData.size();
    m_firmwareCrc = calculateCRC32(firmwareData);
    
    log(QString("Файл прошивки: %1").arg(firmwarePath));
    log(QString("Размер: %1 байт").arg(firmwareSize));
    log(QString("CRC32: 0x%1").arg(QString::number(m_firmwareCrc, 16).toUpper().rightJustified(8, '0')));
    log(QString("Версия: 0x%1").arg(QString::number(fileMetadata.version, 16).toUpper().rightJustified(8, '0')));
    
    log(QString("Используется версия из метаданных: 0x%1").arg(QString::number(fileMetadata.version, 16).toUpper().rightJustified(8, '0')));
    // Если версия не задана, использовать из метаданных файла

    // Проверяем, подключены ли мы к устройству
    if (!isConnected()) {
        log("Ошибка: не подключен к устройству");
        return false;
    }
    
    // Проверяем связь с устройством
    if (!checkConnection()) {
        return false;
    }
    
    // Проверяем текущий статус устройства
    quint32 status;
    quint32 error;
    if (!getDeviceStatus(status, error)) {
        return false;
    }
    
    // Если устройство занято обновлением, предлагаем отменить его
    if (status == UPDATE_STATUS_IN_PROGRESS) {
        log("Устройство уже выполняет обновление. Попытка отмены...");
        if (!abortUpdate()) {
            return false;
        }
    }
    
    // Начинаем процесс обновления
    if (!startUpdate(firmwareSize, fileMetadata.version, isBackup)) {
        return false;
    }
    
    // Отправляем блоки данных
    m_totalBlocks = (firmwareSize + m_blockSize - 1) / m_blockSize;
    log(QString("Отправка %1 блоков по %2 байт...").arg(m_totalBlocks).arg(m_blockSize));
    
    bool success = true;
    for (m_currentBlock = 0; m_currentBlock < m_totalBlocks; m_currentBlock++) {
        quint32 offset = m_currentBlock * m_blockSize;
        quint32 end = qMin(offset + m_blockSize, firmwareSize);
        QByteArray blockData = firmwareData.mid(offset, end - offset);
        
        int progress = (m_currentBlock + 1) * 100 / m_totalBlocks;
        log(QString("Блок %1/%2 (%3 байт) - %4%").arg(m_currentBlock + 1).arg(m_totalBlocks)
                                               .arg(blockData.size()).arg(progress));
        
        emit updateProgress(progress, m_currentBlock + 1, m_totalBlocks);
        
        if (!sendFirmwareBlock(m_currentBlock, blockData)) {
            success = false;
            break;
        }
        
        // Небольшая задержка между блоками
        QThread::msleep(m_retryDelay);
    }
    
    if (success) {
        log("Все блоки успешно отправлены");
        
        // Завершаем процесс обновления
        if (!finishUpdate(m_firmwareCrc)) {
            success = false;
        }
        
        // Проверяем финальный статус
        quint32 finalStatus;
        quint32 finalError;
        if (getDeviceStatus(finalStatus, finalError)) {
            if (finalStatus == UPDATE_STATUS_COMPLETE || finalStatus == UPDATE_STATUS_READY_REBOOT) {
                QString updateType = isBackup ? "резервной" : "основной";
                log(QString("Обновление %1 прошивки успешно завершено").arg(updateType));
                if (!isBackup) {
                    log("Устройство будет перезагружено для применения обновлений");
                } else {
                    log("Устройство готово к использованию резервной прошивки");
                }
                success = true;
            } else {
                log(QString("Обновление завершилось с неожиданным статусом: %1")
                    .arg(getStatusText(finalStatus)));
                success = false;
            }
        }
    }

    emit updateCompleted(success);
    return success;
}

void FirmwareUpdater::onSocketConnected()
{
    log("Сокет подключен");
    emit connectionStatusChanged(true);
}

void FirmwareUpdater::onSocketDisconnected()
{
    log("Сокет отключен");
    resetState();
    emit connectionStatusChanged(false);
}

void FirmwareUpdater::onSocketError(QAbstractSocket::SocketError socketError)
{
    log(QString("Ошибка сокета: %1").arg(m_socket->errorString()));
}

void FirmwareUpdater::onSocketReadyRead()
{
    m_responseBuffer.append(m_socket->readAll());
}

void FirmwareUpdater::onTimeout()
{
    log("Таймаут операции");
}

bool FirmwareUpdater::sendPacket(quint32 command, quint32 blockNumber, quint32 dataSize, const QByteArray &data)
{
    // Создаем заголовок пакета
    QByteArray header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    stream << command;
    stream << dataSize;
    stream << blockNumber;
    
    // Создаем полный пакет
    QByteArray packet = header;
    if (!data.isEmpty()) {
        packet.append(data);
    }
    
    // Очищаем буфер ответа перед отправкой
    m_responseBuffer.clear();
    
    // Отправляем пакет
    qint64 bytesWritten = m_socket->write(packet);
    bool success = (bytesWritten == packet.size());
    
    if (!success) {
        log(QString("Ошибка при отправке данных: %1").arg(m_socket->errorString()));
        return false;
    }
    
    // Принудительная отправка данных
    m_socket->flush();
    
    return true;
}

bool FirmwareUpdater::receiveResponse(quint32 &status, quint32 &error)
{
    // Ожидаем ответ с таймаутом
    m_timeoutTimer->start();
    while (m_responseBuffer.size() < RESPONSE_SIZE) {
        if (!m_socket->waitForReadyRead(10000)) {
            if (m_socket->error() != QAbstractSocket::SocketTimeoutError || m_timeoutTimer->isActive()) {
                log(QString("время ожидания ответа вышло"));
                break;
            }
        }
        QCoreApplication::processEvents();
    }
    m_timeoutTimer->stop();
    
    // Проверяем, получили ли мы полный ответ
    if (m_responseBuffer.size() < RESPONSE_SIZE) {
        log(QString("Ошибка: получен неполный ответ (%1 байт)").arg(m_responseBuffer.size()));
        return false;
    }
    
    // Разбираем ответ
    QDataStream stream(m_responseBuffer);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    quint32 command;
    quint32 reserv;
    stream >> command;
    stream >> status;
    stream >> error;
    stream >> reserv;
    
    // Проверяем, что это действительно ответ
    if (command != CMD_RESPONSE) {
        log(QString("Ошибка: получена неверная команда в ответе: 0x%1")
            .arg(QString::number(command, 16).toUpper().rightJustified(2, '0')));
        return false;
    }
    
    // Очищаем буфер ответа
    m_responseBuffer.clear();
    
    return true;
}

bool FirmwareUpdater::startUpdate(quint32 firmwareSize, quint32 firmwareVersion, bool isBackup)
{
    QString updateType = isBackup ? "резервной" : "основной";
    log(QString("Начало процесса обновления %1 прошивки (размер: %2 байт, версия: 0x%3)...")
        .arg(updateType)
        .arg(firmwareSize)
        .arg(QString::number(firmwareVersion, 16).toUpper().rightJustified(8, '0')));
    
    for (int retry = 0; retry < m_retryCount; retry++) {
        // Определяем команду в зависимости от типа обновления
        quint32 command = isBackup ? CMD_START_BACKUP_UPDATE : CMD_START_UPDATE;

        if (sendPacket(command, firmwareVersion, firmwareSize)) {
            quint32 status;
            quint32 error;
            
            if (receiveResponse(status, error)) {
                if (status == UPDATE_STATUS_IN_PROGRESS) {
                    log("Устройство готово принимать данные");
                    m_isUpdating = true;
                    emit updateStatusChanged(status, error);
                    return true;
                } else {
                    log(QString("Ошибка при начале обновления: %1").arg(getErrorText(error)));
                }
            }
        }
        
        log(QString("Попытка %1/%2 не удалась, повторяем...").arg(retry + 1).arg(m_retryCount));
        QThread::msleep(m_retryDelay);
    }
    
    log("Ошибка: не удалось начать процесс обновления");
    return false;
}

bool FirmwareUpdater::sendFirmwareBlock(quint32 blockNumber, const QByteArray &data)
{
    quint32 crc = calculateCRC32(data);
    quint32 dataSize = data.size();
    
    for (int retry = 0; retry < m_retryCount; retry++) {
        if (sendPacket(CMD_FIRMWARE_DATA, blockNumber, dataSize, data)) {
            quint32 status;
            quint32 error;
            
            if (receiveResponse(status, error)) {
                if (status == UPDATE_STATUS_IN_PROGRESS) {
                    emit updateStatusChanged(status, error);
                    return true;
                } else {
                    log(QString("Ошибка при отправке блока %1: %2")
                        .arg(blockNumber).arg(getErrorText(error)));
                }
            }
        }
        
        log(QString("Попытка %1/%2 не удалась, повторяем...").arg(retry + 1).arg(m_retryCount));
        QThread::msleep(m_retryDelay);
    }
    
    log(QString("Ошибка: не удалось отправить блок %1").arg(blockNumber));
    return false;
}

bool FirmwareUpdater::finishUpdate(quint32 firmwareCrc)
{
    log(QString("Завершение процесса обновления (CRC: 0x%1)...")
        .arg(QString::number(firmwareCrc, 16).toUpper().rightJustified(8, '0')));
    
    for (int retry = 0; retry < m_retryCount; retry++) {
        if (sendPacket(CMD_END_UPDATE, firmwareCrc, 0)) {
            quint32 status;
            quint32 error;
            
            if (receiveResponse(status, error)) {
                if (status == UPDATE_STATUS_COMPLETE) {
                    log("Обновление успешно завершено");
                    m_isUpdating = false;
                    emit updateStatusChanged(status, error);
                    return true;
                } else {
                    log(QString("Ошибка при завершении обновления: %1").arg(getErrorText(error)));
                }
            }
        }
        
        log(QString("Попытка %1/%2 не удалась, повторяем...").arg(retry + 1).arg(m_retryCount));
        QThread::msleep(m_retryDelay);
    }
    
    log("Ошибка: не удалось завершить процесс обновления");
    return false;
}

// для вычисления CRC32
quint32 FirmwareUpdater::calculateCRC32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFF;
    static const quint32 crcTable[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
        0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
        0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
        0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
        0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
        0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
        0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
        0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
        0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
        0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
        0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
        0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
        0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
        0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
        0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
        0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
        0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
        0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
        0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
        0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
        0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
        0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };

    for (int i = 0; i < data.size(); ++i) {
        crc = (crc >> 8) ^ crcTable[(crc ^ data[i]) & 0xFF];
    }

    return ~crc; // Инвертируем результат
}

void FirmwareUpdater::resetState()
{
    m_isUpdating = false;
    m_currentBlock = 0;
    m_totalBlocks = 0;
    m_firmwareCrc = 0;
    m_responseBuffer.clear();
}

QString FirmwareUpdater::getStatusText(quint32 status) const
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

QString FirmwareUpdater::getErrorText(quint32 error) const
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

bool FirmwareUpdater::getDeviceMetadata(FirmwareMetadata &metadata)
{
    log("Запрос метаданных прошивки устройства...");

    if (sendPacket(CMD_GET_METADATA, 0, 0)) {
        // Очищаем буфер и ждём расширенного ответа
        m_responseBuffer.clear();

        // Ожидаем получения данных
        m_timeoutTimer->start();
        while (m_responseBuffer.size() < (int)sizeof(FirmwareMetadata)) {
            if (!m_socket->waitForReadyRead(4000)) {
                if (m_socket->error() != QAbstractSocket::SocketTimeoutError || !m_timeoutTimer->isActive()) {
                    log("Тайм-аут при получении метаданных");
                    break;
                }
            }
            QCoreApplication::processEvents();
        }
        m_timeoutTimer->stop();

        if (m_responseBuffer.size() >= (int)sizeof(FirmwareMetadata)) {
            // Разбираем расширенный ответ
            QDataStream stream(m_responseBuffer);
            stream.setByteOrder(QDataStream::LittleEndian);

            quint32 command, status, error;
            stream >> command >> status >> error;

            if (command == CMD_RESPONSE && status == UPDATE_STATUS_IDLE) {
                // Читаем метаданные
                stream >> metadata.key_start >> metadata.version;
                stream.readRawData(metadata.name_proj, sizeof(metadata.name_proj));
                stream >> metadata.reserved;

                // Проверяем валидность
                if (metadata.key_start == METADATA_KEY) {
                    log(QString("Получены метаданные: %1 версия %2")
                        .arg(QString::fromUtf8(metadata.name_proj))
                        .arg(QString::number(metadata.version, 16).toUpper().rightJustified(8, '0')));

                    emit metadataReceived(metadata);
                    return true;
                } else {
                    log("Получены невалидные метаданные");
                }
            }
        }
    }

    log("Ошибка: не удалось получить метаданные устройства");
    return false;
}

FirmwareMetadata FirmwareUpdater::extractMetadataFromFile(const QString &firmwarePath)
{
    FirmwareMetadata metadata;
    memset(&metadata, 0, sizeof(metadata));

    QFile file(firmwarePath);
    if (!file.open(QIODevice::ReadOnly)) {
        log(QString("Ошибка открытия файла: %1").arg(firmwarePath));
        return metadata;
    }

    // Читаем файл с позиции METADATA_OFFSET (512 байт)
    if (file.size() > (qint64)(512 + sizeof(FirmwareMetadata))) {
        file.seek(512);
        QByteArray data = file.read(sizeof(FirmwareMetadata));

        if (data.size() == sizeof(FirmwareMetadata)) {
            QDataStream stream(data);
            stream.setByteOrder(QDataStream::LittleEndian);

            stream >> metadata.key_start >> metadata.version;
            stream.readRawData(metadata.name_proj, sizeof(metadata.name_proj));
            stream >> metadata.reserved;

            // Проверяем валидность
            if (metadata.key_start == METADATA_KEY) {
                log(QString("Метаданные из файла: %1 версия %2")
                    .arg(QString::fromUtf8(metadata.name_proj))
                    .arg(QString::number(metadata.version, 16).toUpper().rightJustified(8, '0')));
            } else {
                log("В файле не найдены валидные метаданные");
                memset(&metadata, 0, sizeof(metadata));
            }
        }
    } else {
        log("Файл слишком мал для содержания метаданных");
    }

    file.close();
    return metadata;
}

void FirmwareUpdater::log(const QString &message)
{
    qDebug().noquote() << "[FirmwareUpdater]" << message;
    //qDebug().noquote() << QString("[FirmwareUpdater]") << QString::fromUtf8(message.toUtf8());
    emit logMessage(message);
}
