import socket

HOST, PORT = '127.0.0.1', 65432
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind((HOST, PORT))

s.listen(1)
c, addr = s.accept()

while not(int(c.recv(8))):
    pass

c.close()
s.close()
