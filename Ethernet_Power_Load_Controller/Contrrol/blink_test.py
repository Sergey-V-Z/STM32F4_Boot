from socket import *
import sys
from datetime import datetime
import time

# Адрес центральной платы
host = '192.168.1.101'
port = 81
addr = (host, port)

tcp_socket = socket(AF_INET, SOCK_STREAM)
tcp_socket.connect(addr)


data = "C5A1D10N0x" #все каналы на 1%
tcp_socket.send(data.encode())
time.sleep(0.03)

# 1
data = "C4A0D1N2x" #вкл канал
tcp_socket.send(data.encode())
time.sleep(1)
data = "C4A0D0N2x" #выкл канал
tcp_socket.send(data.encode())
time.sleep(1)
# 2
data = "C4A0D1N1x" #вкл канал
tcp_socket.send(data.encode())
time.sleep(1)
data = "C4A0D0N1x" #выкл канал
tcp_socket.send(data.encode())
time.sleep(1)
# 3
data = "C4A0D1N0x" #вкл канал
tcp_socket.send(data.encode())
time.sleep(1)
data = "C4A0D0N0x" #выкл канал
tcp_socket.send(data.encode())
time.sleep(1)
# 4
data = "C4A0D1N5x" #вкл канал
tcp_socket.send(data.encode())
time.sleep(1)
data = "C4A0D0N5x" #выкл канал
tcp_socket.send(data.encode())
time.sleep(1)
# 5
data = "C4A0D1N4x" #вкл канал
tcp_socket.send(data.encode())
time.sleep(1)
data = "C4A0D0N4x" #выкл канал
tcp_socket.send(data.encode())
time.sleep(1)
# 6
data = "C4A0D1N3x" #вкл канал
tcp_socket.send(data.encode())
time.sleep(1)
data = "C4A0D0N3x" #выкл канал
tcp_socket.send(data.encode())
time.sleep(1)

#data = bytes.decode(data)
data = tcp_socket.recv(1024)
print(data)

tcp_socket.close()
