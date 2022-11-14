import socket

HOST, PORT = '127.0.0.1', 65432
c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
c.connect((HOST, PORT))

c.send(b'1')
c.close()
