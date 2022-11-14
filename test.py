# Test server for key-value store

import socket
import utils

HOST, PORT = '127.0.0.1', 65432
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind((HOST, PORT))

s.listen(1)
c, addr = s.accept()

data = utils.hash_table()
print(data)

while not(int(c.recv(8))):
    pass

c.close()
s.close()
