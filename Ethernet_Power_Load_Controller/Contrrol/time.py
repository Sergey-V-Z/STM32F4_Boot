from datetime import datetime
import time
from socket import *
import sys

# Адрес центральной платы
host = '192.168.0.10'
port = 81
addr = (host, port)

tcp_socket = socket(AF_INET, SOCK_STREAM)
tcp_socket.connect(addr)

#data = input('write to server: ')
#if not data:
#    tcp_socket.close()
#    sys.exit(1)

# encode - перекодирует введенные данные в байты, decode - обратно
#data = str.encode(data)

# Флаги для формирования сообщения
comandFlag = "C"
addresFlag = "A"
dataFlag = "D"
endFlag = "x"

# комманды
cmd1 = "11"

# адрес переменных
#Addr1 = "4"
#Addr2 = "3"
#Addr3 = "2"

# данные для записи
wdata1 = "0"


# data = int(data)
# собираем сообщение
data = comandFlag
data += cmd1
data += addresFlag
data += dataFlag
data += wdata1
data += endFlag

start_time = time.time()
tcp_socket.send(data.encode())
data_r = tcp_socket.recv(1024)

print('data:')
print(data_r)

tcp_socket.close()
