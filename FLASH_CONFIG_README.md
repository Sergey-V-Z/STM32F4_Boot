# Flash Configuration Files

## Назначение

Конфигурационные файлы `flash_config.json` определяют карту памяти для каждого устройства и указывают какие файлы прошивок в какие области памяти записывать.

## Структура файла

```json
{
  "device_type": "Имя устройства",
  "device_id": 1,
  "description": "Описание устройства",
  "mcu": "STM32F407VG",
  "flash_layout": {
    "region_name": {
      "file": "имя_файла.bin",
      "address": "0x08000000",
      "size": "0xC000",
      "description": "Описание области",
      "source": "путь/к/файлу.bin",
      "optional": false
    }
  },
  "verify_after_flash": true,
  "reset_after_flash": true
}
```

## Карта памяти STM32F407VG

### Текущая раскладка:

| Область      | Адрес начала | Размер | Описание                    |
|-------------|--------------|--------|-----------------------------|
| Bootloader  | 0x08000000   | 48KB   | Загрузчик                   |
| Boot Data   | 0x0800C000   | 16KB   | Конфигурация загрузчика     |
| Application | 0x08010000   | 448KB  | Основное приложение         |

### Линкер скрипт (из bootloader):

```
MEMORY {
  FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 48K   /* Bootloader */
  BOOT_DATA (rx)  : ORIGIN = 0x0800C000, LENGTH = 16K   /* Boot config */
  APP (rx)        : ORIGIN = 0x08010000, LENGTH = 448K  /* Application */
  RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 128K
  CCMRAM (xrw)    : ORIGIN = 0x10000000, LENGTH = 64K
}
```

## Использование

1. Поместите `flash_config.json` в папку с проектом
2. В конфигурации укажите пути к `.bin` файлам
3. Settings Tool автоматически найдет и распарсит конфигурацию
4. При прошивке будут записаны все указанные регионы

## Примеры

### Полная прошивка (bootloader + application):
```json
{
  "flash_layout": {
    "bootloader": {
      "file": "bootloader.bin",
      "address": "0x08000000"
    },
    "application": {
      "file": "app.bin",
      "address": "0x08010000"
    }
  }
}
```

### Только application (bootloader уже прошит):
```json
{
  "flash_layout": {
    "application": {
      "file": "app.bin",
      "address": "0x08010000"
    }
  }
}
```

## Device ID

| ID | Устройство        |
|----|-------------------|
| 0  | Bootloader only   |
| 1  | Control_PWR       |
| 2  | UD_v4             |
| 3  | 6ch_LED_Driver    |
| 5  | IR_Sensor         |
