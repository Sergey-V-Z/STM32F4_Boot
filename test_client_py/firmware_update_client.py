import socket
import struct
import binascii
import os
import time
import sys

# Константы
CMD_START_UPDATE = 0x01
CMD_FIRMWARE_DATA = 0x02
CMD_END_UPDATE = 0x03
CMD_GET_STATUS = 0x04
CMD_ABORT_UPDATE = 0x05
CMD_RESPONSE = 0x80

# Статусы
UPDATE_STATUS_IDLE = 0x00
UPDATE_STATUS_IN_PROGRESS = 0x01
UPDATE_STATUS_COMPLETE = 0x02
UPDATE_STATUS_ERROR = 0x03
UPDATE_STATUS_READY_REBOOT = 0x04

def calculate_crc32(data):
    return binascii.crc32(data) & 0xFFFFFFFF

def send_packet(sock, command, block_number, data_size, crc, data=None):
    # Изменён формат структуры, 'H' (uint16_t) заменён на 'I' (uint32_t) для поля data_size
    header = struct.pack('<BIII', command, block_number, data_size, crc)
    if data:
        packet = header + data
    else:
        packet = header
    sock.sendall(packet)
    
def receive_response(sock):
    response = sock.recv(1024)
    if len(response) < 10:
        print("Received invalid response")
        return None
    
    cmd, status, block_number, error = struct.unpack('<BBII', response[:10])
    return {'command': cmd, 'status': status, 'block_number': block_number, 'error': error}

def update_firmware(device_ip, firmware_path):
    # Открываем файл прошивки
    with open(firmware_path, 'rb') as f:
        firmware_data = f.read()
    
    # Получаем размер и CRC
    firmware_size = len(firmware_data)
    firmware_crc = calculate_crc32(firmware_data)
    firmware_version = 0x00010000  # Версия 1.0.0.0
    
    print(f"Firmware size: {firmware_size} bytes, CRC: 0x{firmware_crc:08X}")
    
    # Подключаемся к устройству
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(15)  # Увеличенный таймаут для надёжности
    
    try:
        print(f"Connecting to {device_ip}:8080...")
        sock.connect((device_ip, 8080))
        
        # Отправляем команду начала обновления
        print("Starting firmware update...")
        send_packet(sock, CMD_START_UPDATE, firmware_version, firmware_size, 0)
        response = receive_response(sock)
        
        if not response:
            print("No response from device")
            return False
            
        if response['status'] != UPDATE_STATUS_IN_PROGRESS:
            print(f"Failed to start update: Status={response['status']}, Error={response['error']}")
            return False
        
        print("Update started successfully")
        
        # Отправляем блоки данных
        block_size = 1024
        blocks_count = (firmware_size + block_size - 1) // block_size
        
        for i in range(blocks_count):
            offset = i * block_size
            end = min(offset + block_size, firmware_size)
            data = firmware_data[offset:end]
            block_crc = calculate_crc32(data)
            
            send_packet(sock, CMD_FIRMWARE_DATA, i, len(data), block_crc, data)
            response = receive_response(sock)
            
            if not response:
                print(f"No response after sending block {i+1}/{blocks_count}")
                return False
                
            if response['status'] != UPDATE_STATUS_IN_PROGRESS:
                print(f"Failed to send block {i+1}/{blocks_count}: Status={response['status']}, Error={response['error']}")
                return False
            
            print(f"Block {i+1}/{blocks_count} sent successfully ({len(data)} bytes)")
            
            # Небольшая задержка для обработки данных на устройстве
            time.sleep(0.01)
        
        # Завершаем обновление
        print("Finalizing update...")
        send_packet(sock, CMD_END_UPDATE, 0, 0, firmware_crc)
        response = receive_response(sock)
        
        if not response:
            print("No response when finalizing update")
            return False
            
        if response['status'] != UPDATE_STATUS_COMPLETE:
            print(f"Failed to complete update: Status={response['status']}, Error={response['error']}")
            return False
        
        print("Firmware update completed successfully")
        print("Device will reboot to apply the update")
        return True
        
    except socket.timeout:
        print("Connection timed out")
        return False
    except ConnectionRefusedError:
        print("Connection refused. Make sure device is running and correct IP address is provided")
        return False
    except Exception as e:
        print(f"Error: {e}")
        return False
    finally:
        sock.close()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <device_ip> <firmware_file>")
        sys.exit(1)
    
    device_ip = sys.argv[1]
    firmware_path = sys.argv[2]
    
    if not os.path.exists(firmware_path):
        print(f"Error: Firmware file '{firmware_path}' not found")
        sys.exit(1)
    
    success = update_firmware(device_ip, firmware_path)
    if success:
        print("Update completed successfully")
    else:
        print("Update failed")