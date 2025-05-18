#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "firmware_updater.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Слоты для обработки действий пользователя
    void onConnectClicked();
    void onStatusClicked();
    void onSelectFirmwareClicked();
    void onUpdateClicked();
    void onAbortClicked();
    void onMetadataReceived(const FirmwareMetadata &metadata);
    void onFirmwareSelected(const QString &firmwarePath);
    void onMetadataMismatch(const QString &fileProject, const QString &deviceProject);

    // Слоты для обработки событий от FirmwareUpdater
    void onUpdateProgress(int percent, quint32 currentBlock, quint32 totalBlocks);
    void onUpdateStatusChanged(quint8 status, quint32 error);
    void onLogMessage(const QString &message);

private:
    Ui::MainWindow *ui;

    // Обновлятор прошивки
    FirmwareUpdater *m_updater;

    // Вспомогательные методы
    void updateStatusDisplay(quint8 status, quint32 error);
    void setControlsEnabled(bool enabled);
    QString getStatusText(quint8 status);
    QString getErrorText(quint32 error);
};
#endif // MAINWINDOW_H
