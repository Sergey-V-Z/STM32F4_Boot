/**
 * @file firmware_updater.h
 * @brief Заголовочный файл клиента обновления прошивки
 */

#ifndef FIRMWARE_UPDATER_H
#define FIRMWARE_UPDATER_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QTimer>
#include <QString>
#include <QFile>

// Команды протокола обновления
#define CMD_PING                	0x00000001    // Проверка связи
#define CMD_START_UPDATE        	0x00000002    // Начало процесса обновления
#define CMD_FIRMWARE_DATA       	0x00000003    // Блок данных прошивки
#define CMD_END_UPDATE          	0x00000004    // Завершение процесса обновления
#define CMD_GET_STATUS          	0x00000005    // Запрос статуса обновления
#define CMD_ABORT_UPDATE        	0x00000006    // Отмена процесса обновления
#define CMD_START_BACKUP_UPDATE   	0x00000007    // Начало процесса обновления резервной прошивки
#define CMD_GET_METADATA            0x00000008    // Получить метаданные текущей прошивки
#define CMD_RESPONSE            	0x00000080    // Ответ сервера

// Статусы
#define UPDATE_STATUS_IDLE          0x00000000
#define UPDATE_STATUS_IN_PROGRESS   0x00000001
#define UPDATE_STATUS_COMPLETE      0x00000002
#define UPDATE_STATUS_ERROR         0x00000003
#define UPDATE_STATUS_READY_REBOOT  0x00000004

// Коды ошибок
#define UPDATE_ERROR_NONE           0x00000000
#define UPDATE_ERROR_INVALID_CMD    0x00000001
#define UPDATE_ERROR_INVALID_SIZE   0x00000002
#define UPDATE_ERROR_FLASH_FAILURE  0x00000003
#define UPDATE_ERROR_CRC_MISMATCH   0x00000004
#define UPDATE_ERROR_SEQ_ERROR      0x00000005
#define UPDATE_ERROR_BUSY           0x00000006
#define UPDATE_ERROR_ABORT          0x00000007

// Размеры структур
#define HEADER_SIZE             12  // command(4) + size(4) + block_number(4)
#define RESPONSE_SIZE           16  // command(4) + status(4) + error(4) + reserv(4)
#define METADATA_RESPONSE_SIZE  sizeof(MetadataResponsePacket)
#define MAX_RESPONSE_SIZE       sizeof(MetadataResponsePacket) // Максимальный размер ответа

// Настройки клиента
#define DEFAULT_PORT       8080
#define DEFAULT_BLOCK_SIZE 256
#define DEFAULT_TIMEOUT    30000 // в миллисекундах
#define DEFAULT_RETRY_COUNT 3
#define DEFAULT_DELAY      1 // в миллисекундах

// Определения для метаданных
#define METADATA_KEY            0xDEADBEEF

// Структура метаданных
typedef struct {
    uint32_t key_start;       // Магическое число (0xDEADBEEF)
    uint32_t version;         // Версия прошивки
    uint8_t name_proj[140];   // Название проекта или устройства
    uint32_t reserved;        // Зарезервировано для будущего использования
} meta_t;

// Cтруктуру ответа с метаданными
struct FirmwareMetadata {
    quint32 key_start;        // Магическое число
    quint32 version;          // Версия прошивки
    char name_proj[140];      // Название проекта
    quint32 reserved;         // Резерв
};

class FirmwareUpdater : public QObject
{
    Q_OBJECT

public:
    explicit FirmwareUpdater(QObject *parent = nullptr);
    ~FirmwareUpdater();

    // Методы для работы с устройством
    bool connectToDevice(const QString &hostAddress, quint16 port = DEFAULT_PORT);
    void disconnectFromDevice();
    bool isConnected() const;
    bool checkConnection();
    bool getDeviceStatus(quint32 &status, quint32 &error);
    bool abortUpdate();
    bool getDeviceMetadata(FirmwareMetadata &metadata);
    FirmwareMetadata extractMetadataFromFile(const QString &firmwarePath);

    // Метод для обновления прошивки
    bool updateFirmware(const QString &firmwarePath, bool isBackup = false);

signals:
    // Сигналы для уведомления о прогрессе
    void connectionStatusChanged(bool connected);
    void updateProgress(int percent, quint32 blockNumber, quint32 totalBlocks);
    void updateStatusChanged(quint32 status, quint32 error);
    void updateCompleted(bool success);
    void deviceStatusReceived(quint32 status, quint32 error);
    void logMessage(const QString &message);
    void metadataReceived(const FirmwareMetadata &metadata);
    void metadataMismatch(const QString &fileProject, const QString &deviceProject);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onSocketReadyRead();
    void onTimeout();

private:
    // Методы для работы с протоколом
    bool sendPacket(quint32 command, quint32 blockNumber, quint32 dataSize, const QByteArray &data = QByteArray());
    bool receiveResponse(quint32 &status, quint32 &error);
    bool startUpdate(quint32 firmwareSize, quint32 firmwareVersion, bool isBackup);
    bool sendFirmwareBlock(quint32 blockNumber, const QByteArray &data);
    bool finishUpdate(quint32 firmwareCrc);
    
    // Метод для расчета CRC32
    quint32 calculateCRC32(const QByteArray &data);
    
    // Вспомогательные методы
    void resetState();
    QString getStatusText(quint32 status) const;
    QString getErrorText(quint32 error) const;
    void log(const QString &message);

    // Переменные для работы с сокетом
    QTcpSocket *m_socket;
    QTimer *m_timeoutTimer;
    QByteArray m_responseBuffer;
    
    // Состояние обновления
    bool m_isUpdating;
    int m_retryCount;
    int m_retryDelay;
    quint32 m_blockSize;
    quint32 m_currentBlock;
    quint32 m_totalBlocks;
    quint32 m_firmwareCrc;
};

#endif // FIRMWARE_UPDATER_H
