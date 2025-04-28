#!/usr/bin/env python3
"""
Клиент обновления прошивки для работы с упрощенным протоколом
"""
import socket
import struct
import binascii
import os
import time
import sys
import argparse

# Константы протокола
CMD_PING = 0x01
CMD_START_UPDATE = 0x02
CMD_FIRMWARE_DATA = 0x03
CMD_END_UPDATE = 0x04
CMD_GET_STATUS = 0x05
CMD_ABORT_UPDATE = 0x06
CMD_RESPONSE = 0x80

# Статусы
UPDATE_STATUS_IDLE = 0x00
UPDATE_STATUS_IN_PROGRESS = 0x01
UPDATE_STATUS_COMPLETE = 0x02
UPDATE_STATUS_ERROR = 0x03
UPDATE_STATUS_READY_REBOOT = 0x04

# Коды ошибок
UPDATE_ERROR_NONE = 0x00
UPDATE_ERROR_INVALID_CMD = 0x01
UPDATE_ERROR_INVALID_SIZE = 0x02
UPDATE_ERROR_FLASH_FAILURE = 0x03
UPDATE_ERROR_CRC_MISMATCH = 0x04
UPDATE_ERROR_SEQ_ERROR = 0x05
UPDATE_ERROR_BUSY = 0x06
UPDATE_ERROR_ABORT = 0x07

# Настройки клиента
DEFAULT_PORT = 8080
DEFAULT_BLOCK_SIZE = 512
DEFAULT_TIMEOUT = 15
DEFAULT_RETRY_COUNT = 3
DEFAULT_DELAY = 0.1

# Размеры структур
HEADER_SIZE = 9  # command(1) + size(4) + block_number(4)
RESPONSE_SIZE = 6  # command(1) + status(1) + error(4)

# Статус коды
STATUS_TEXTS = {
    UPDATE_STATUS_IDLE: "Ожидание",
    UPDATE_STATUS_IN_PROGRESS: "В процессе обновления",
    UPDATE_STATUS_COMPLETE: "Обновление завершено",
    UPDATE_STATUS_ERROR: "Ошибка",
    UPDATE_STATUS_READY_REBOOT: "Готов к перезагрузке"
}

# Тексты ошибок
ERROR_TEXTS = {
    UPDATE_ERROR_NONE: "Нет ошибок",
    UPDATE_ERROR_INVALID_CMD: "Неверная команда",
    UPDATE_ERROR_INVALID_SIZE: "Неверный размер",
    UPDATE_ERROR_FLASH_FAILURE: "Ошибка записи во флеш",
    UPDATE_ERROR_CRC_MISMATCH: "Ошибка CRC",
    UPDATE_ERROR_SEQ_ERROR: "Ошибка последовательности",
    UPDATE_ERROR_BUSY: "Устройство занято",
    UPDATE_ERROR_ABORT: "Обновление отменено"
}

def calculate_crc32(data):
    """Рассчитывает CRC32 для блока данных"""
    return binascii.crc32(data) & 0xFFFFFFFF

def send_packet(sock, command, block_number, data_size, data=None):
    """Отправляет пакет с заголовком и данными (если есть)"""
    # Формируем заголовок
    header = struct.pack('<BII', command, data_size, block_number)
    
    if data:
        packet = header + data
    else:
        packet = header
    
    try:
        print(packet)
        sock.sendall(packet)
        time.sleep(0.01)
        print(f"Отправлено {len(packet)} байт. Команда: 0x{command:02X}, блок: {block_number}, размер: {data_size}")

        return True
    except socket.timeout:
        print("Ошибка: таймаут при отправке данных")
        return False
    except Exception as e:
        print(f"Ошибка при отправке данных: {e}")
        return False

def receive_response(sock, timeout=DEFAULT_TIMEOUT):
    """Получает и разбирает ответ от сервера"""
    # Устанавливаем таймаут для операции приема
    sock.settimeout(timeout)
    
    try:
        # Получаем ответ
        response = sock.recv(RESPONSE_SIZE)
        if len(response) < RESPONSE_SIZE:
            print(f"Ошибка: получен неполный ответ ({len(response)} байт)")
            return None
        
        # Разбираем ответ
        cmd, status, error = struct.unpack('<BBI', response)
        
        return {
            'command': cmd,
            'status': status,
            'error': error
        }
    except socket.timeout:
        print("Ошибка: таймаут при ожидании ответа")
        return None
    except struct.error as e:
        print(f"Ошибка при разборе ответа: {e}")
        return None
    except Exception as e:
        print(f"Ошибка при получении ответа: {e}")
        return None

def check_connection(sock):
    """Проверяет связь с устройством с помощью команды PING"""
    print("Проверка связи с устройством...")
    
    for retry in range(DEFAULT_RETRY_COUNT):
        if send_packet(sock, CMD_PING, 0, 0):
            response = receive_response(sock)
            if response and response['command'] == CMD_RESPONSE:
                print(f"Устройство отвечает, статус: {STATUS_TEXTS.get(response['status'], 'Неизвестный')}")
                return True
        
        print(f"Попытка {retry+1}/{DEFAULT_RETRY_COUNT} не удалась, повторяем...")
        time.sleep(DEFAULT_DELAY)
    
    print("Ошибка: устройство не отвечает на команду PING")
    return False

def get_device_status(sock):
    """Запрашивает и выводит текущий статус устройства"""
    if send_packet(sock, CMD_GET_STATUS, 0, 0):
        response = receive_response(sock)
        if response and response['command'] == CMD_RESPONSE:
            status = response['status']
            error = response['error']
            
            print(f"Статус устройства: {STATUS_TEXTS.get(status, f'Неизвестный ({status})')}")
            if error != UPDATE_ERROR_NONE:
                print(f"Код ошибки: {ERROR_TEXTS.get(error, f'Неизвестная ошибка ({error})')}")
            
            return status, error
    
    print("Ошибка: не удалось получить статус устройства")
    return None, None

def start_update(sock, firmware_size, firmware_version):
    """Начинает процесс обновления"""
    print(f"Начало процесса обновления (размер: {firmware_size} байт, версия: 0x{firmware_version:08X})...")
    
    for retry in range(DEFAULT_RETRY_COUNT):
        if send_packet(sock, CMD_START_UPDATE, firmware_version, firmware_size):
            response = receive_response(sock)
            if response and response['command'] == CMD_RESPONSE:
                if response['status'] == UPDATE_STATUS_IN_PROGRESS:
                    print("Устройство готово принимать данные")
                    return True
                else:
                    error = response['error']
                    print(f"Ошибка при начале обновления: {ERROR_TEXTS.get(error, f'Неизвестная ошибка ({error})')}")
        
        print(f"Попытка {retry+1}/{DEFAULT_RETRY_COUNT} не удалась, повторяем...")
        time.sleep(DEFAULT_DELAY)
    
    print("Ошибка: не удалось начать процесс обновления")
    return False

def send_firmware_block(sock, block_number, data):
    """Отправляет блок данных прошивки"""
    crc = calculate_crc32(data)
    data_size = len(data)
    
    for retry in range(DEFAULT_RETRY_COUNT):
        if send_packet(sock, CMD_FIRMWARE_DATA, block_number, data_size, data):
            response = receive_response(sock)
            if response and response['command'] == CMD_RESPONSE:
                if response['status'] == UPDATE_STATUS_IN_PROGRESS:
                    return True
                else:
                    error = response['error']
                    print(f"Ошибка при отправке блока {block_number}: {ERROR_TEXTS.get(error, f'Неизвестная ошибка ({error})')}")
        
        print(f"Попытка {retry+1}/{DEFAULT_RETRY_COUNT} не удалась, повторяем...")
        time.sleep(DEFAULT_DELAY)
    
    print(f"Ошибка: не удалось отправить блок {block_number}")
    return False

def finish_update(sock, firmware_crc):
    """Завершает процесс обновления"""
    print(f"Завершение процесса обновления (CRC: 0x{firmware_crc:08X})...")
    
    for retry in range(DEFAULT_RETRY_COUNT):
        if send_packet(sock, CMD_END_UPDATE, firmware_crc, 0):
            response = receive_response(sock)
            if response and response['command'] == CMD_RESPONSE:
                if response['status'] == UPDATE_STATUS_COMPLETE:
                    print("Обновление успешно завершено")
                    return True
                else:
                    error = response['error']
                    print(f"Ошибка при завершении обновления: {ERROR_TEXTS.get(error, f'Неизвестная ошибка ({error})')}")
        
        print(f"Попытка {retry+1}/{DEFAULT_RETRY_COUNT} не удалась, повторяем...")
        time.sleep(DEFAULT_DELAY)
    
    print("Ошибка: не удалось завершить процесс обновления")
    return False

def abort_update(sock):
    """Отменяет текущий процесс обновления"""
    print("Отмена процесса обновления...")
    
    if send_packet(sock, CMD_ABORT_UPDATE, 0, 0):
        response = receive_response(sock)
        if response and response['command'] == CMD_RESPONSE:
            if response['status'] == UPDATE_STATUS_IDLE:
                print("Процесс обновления успешно отменен")
                return True
            else:
                error = response['error']
                print(f"Ошибка при отмене обновления: {ERROR_TEXTS.get(error, f'Неизвестная ошибка ({error})')}")
    
    print("Ошибка: не удалось отменить процесс обновления")
    return False

def update_firmware(device_ip, port, firmware_path, block_size, firmware_version=0x00010000):
    """Основная функция обновления прошивки"""
    # Открываем файл прошивки
    try:
        with open(firmware_path, 'rb') as f:
            firmware_data = f.read()
    except Exception as e:
        print(f"Ошибка при чтении файла прошивки: {e}")
        return False
    
    # Получаем размер и CRC
    firmware_size = len(firmware_data)
    firmware_crc = calculate_crc32(firmware_data)
    
    print(f"Файл прошивки: {firmware_path}")
    print(f"Размер: {firmware_size} байт")
    print(f"CRC32: 0x{firmware_crc:08X}")
    print(f"Версия: 0x{firmware_version:08X}")
    
    # Подключаемся к устройству
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(DEFAULT_TIMEOUT)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    try:
        print(f"Подключение к {device_ip}:{port}...")
        sock.connect((device_ip, port))
        print("Подключение установлено")
        
        # Проверяем связь с устройством
        if not check_connection(sock):
            return False
        
        # Проверяем текущий статус устройства
        status, error = get_device_status(sock)
        if status is None:
            return False
        
        # Если устройство занято обновлением, предлагаем отменить его
        if status == UPDATE_STATUS_IN_PROGRESS:
            choice = input("Устройство уже выполняет обновление. Отменить? (y/n): ")
            if choice.lower() == 'y':
                if not abort_update(sock):
                    return False
            else:
                print("Обновление отменено пользователем")
                return False
        
        # Начинаем процесс обновления
        if not start_update(sock, firmware_size, firmware_version):
            return False
        
        # Отправляем блоки данных
        blocks_count = (firmware_size + block_size - 1) // block_size
        print(f"Отправка {blocks_count} блоков по {block_size} байт...")
        
        for i in range(blocks_count):
            offset = i * block_size
            end = min(offset + block_size, firmware_size)
            data = firmware_data[offset:end]
            
            progress = (i + 1) / blocks_count * 100
            print(f"Блок {i+1}/{blocks_count} ({len(data)} байт) - {progress:.1f}%", end='\r')
            
            if not send_firmware_block(sock, i, data):
                return False
            
            # Небольшая задержка между блоками
            time.sleep(DEFAULT_DELAY)
        
        print("\nВсе блоки успешно отправлены")
        
        # Завершаем процесс обновления
        if not finish_update(sock, firmware_crc):
            return False
        
        # Проверяем финальный статус
        final_status, final_error = get_device_status(sock)
        if final_status == UPDATE_STATUS_COMPLETE or final_status == UPDATE_STATUS_READY_REBOOT:
            print("Обновление прошивки успешно завершено")
            print("Устройство будет перезагружено для применения обновлений")
            return True
        else:
            print(f"Обновление завершилось с неожиданным статусом: {STATUS_TEXTS.get(final_status, 'Неизвестный')}")
            return False
        
    except socket.timeout:
        print("Ошибка: таймаут подключения")
        return False
    except ConnectionRefusedError:
        print("Ошибка: подключение отклонено. Убедитесь, что устройство включено и IP-адрес указан верно")
        return False
    except Exception as e:
        print(f"Ошибка: {e}")
        return False
    finally:
        sock.close()
        print("Соединение закрыто")

def parse_arguments():
    parser = argparse.ArgumentParser(description='Клиент обновления прошивки для STM32')
    parser.add_argument('ip', help='IP-адрес устройства')
    parser.add_argument('firmware', help='Путь к файлу прошивки')
    parser.add_argument('-p', '--port', type=int, default=DEFAULT_PORT, help=f'Порт (по умолчанию: {DEFAULT_PORT})')
    parser.add_argument('-b', '--block-size', type=int, default=DEFAULT_BLOCK_SIZE, help=f'Размер блока данных (по умолчанию: {DEFAULT_BLOCK_SIZE})')
    parser.add_argument('-v', '--version', type=lambda x: int(x, 0), default=0x00010000, help='Версия прошивки в HEX формате (по умолчанию: 0x00010000)')
    parser.add_argument('-t', '--timeout', type=int, default=DEFAULT_TIMEOUT, help=f'Таймаут операций в секундах (по умолчанию: {DEFAULT_TIMEOUT})')
    parser.add_argument('-d', '--delay', type=float, default=DEFAULT_DELAY, help=f'Задержка между блоками в секундах (по умолчанию: {DEFAULT_DELAY})')
    parser.add_argument('-r', '--retry', type=int, default=DEFAULT_RETRY_COUNT, help=f'Количество повторных попыток (по умолчанию: {DEFAULT_RETRY_COUNT})')
    parser.add_argument('-s', '--status', action='store_true', help='Только запросить статус устройства')
    parser.add_argument('-a', '--abort', action='store_true', help='Отменить текущее обновление')
    
    return parser.parse_args()

def main():
    args = parse_arguments()
    
    # Обновляем глобальные параметры
    global DEFAULT_TIMEOUT, DEFAULT_RETRY_COUNT, DEFAULT_DELAY
    DEFAULT_TIMEOUT = args.timeout
    DEFAULT_RETRY_COUNT = args.retry
    DEFAULT_DELAY = args.delay
    
    # Подключаемся к устройству
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(DEFAULT_TIMEOUT)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    try:
        print(f"Подключение к {args.ip}:{args.port}...")
        sock.connect((args.ip, args.port))
        print("Подключение установлено")
        
        # Если нужно только запросить статус
        if args.status:
            check_connection(sock)
            get_device_status(sock)
            return 0
        
        # Если нужно только отменить обновление
        if args.abort:
            if check_connection(sock) and abort_update(sock):
                return 0
            else:
                return 1
        
        # Проверяем наличие файла прошивки
        if not os.path.exists(args.firmware):
            print(f"Ошибка: файл прошивки '{args.firmware}' не найден")
            return 1
        
        # Выполняем обновление прошивки
        success = update_firmware(args.ip, args.port, args.firmware, args.block_size, args.version)
        return 0 if success else 1
        
    except socket.timeout:
        print("Ошибка: таймаут подключения")
        return 1
    except ConnectionRefusedError:
        print("Ошибка: подключение отклонено. Убедитесь, что устройство включено и IP-адрес указан верно")
        return 1
    except Exception as e:
        print(f"Ошибка: {e}")
        return 1
    finally:
        sock.close()
        print("Соединение закрыто")

if __name__ == "__main__":
    sys.exit(main())