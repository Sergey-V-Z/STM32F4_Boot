from datetime import datetime
import time
from socket import *
import sys

# Адрес центральной платы
host = '192.168.0.14'
host2 = '192.168.0.1'
port = 81
addr = (host, port)
addr2 = (host2, port)

tcp_socket = socket(AF_INET, SOCK_STREAM)
tcp_socket.connect(addr)

tcp_socket2 = socket(AF_INET, SOCK_STREAM)
tcp_socket2.connect(addr2)

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
cmd1 = "17"
cmd2 = "3"
cmd3 = "4"
cmd4 = "1"
cmd5 = "2"
cmd6 = "8"
# адрес переменных
#Addr1 = "4"
#Addr2 = "3"
#Addr3 = "2"

# данные для записи
wdata1 = "0"
wdata2 = "1"
wdata3 = "1"
wdata4 = "0"
wdata5 = "0"
wdata6 = "0"

# data = int(data)
# собираем сообщение
data = comandFlag
data += cmd1
data += addresFlag
data += dataFlag
data += wdata1
data += endFlag

data2 = comandFlag
data2 += cmd2
data2 += addresFlag
data2 += dataFlag
data2 += wdata2
data2 += endFlag

data2 += comandFlag
data2 += cmd3
data2 += addresFlag
data2 += dataFlag
data2 += wdata3
data2 += endFlag

data3 = comandFlag
data3 += cmd4
data3 += addresFlag
data3 += dataFlag
data3 += wdata4
data3 += endFlag

data3 += comandFlag
data3 += cmd5
data3 += addresFlag
data3 += dataFlag
data3 += wdata5
data3 += endFlag

if 1:
    start_time = time.time()
    tcp_socket.send(data.encode())
    #data = bytes.decode(data)
    data_r = tcp_socket.recv(1024)
    tcp_socket2.send(data.encode())
    data_r2 = tcp_socket2.recv(1024)
    #time.sleep(1)
    print('data:')
    print(time.time() - start_time)
    print(data_r)
    print(data_r2)

    time.sleep(5)

start_time = time.time()
tcp_socket.send(data2.encode())
#data = bytes.decode(data)
data_r3 = tcp_socket.recv(1024)
tcp_socket2.send(data2.encode())
data_r4 = tcp_socket2.recv(1024)
#time.sleep(1)

print(data_r3)
print(data_r4)

time.sleep(0.03)

tcp_socket.send(data3.encode())
#data = bytes.decode(data)
data_r5 = tcp_socket.recv(1024)
tcp_socket2.send(data3.encode())
data_r6 = tcp_socket2.recv(1024)
#time.sleep(1)
print('data2:')
print(time.time() - start_time)
print(data_r5)
print(data_r6)

tcp_socket.close()
