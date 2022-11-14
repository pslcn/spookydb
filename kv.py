import socket
import hashlib

HOST, PORT = '127.0.0.1', 65432

c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
c.connect((HOST, PORT))

def hash_table():
    h = {}

    h[hashlib.md5('0'.encode()).hexdigest()] = 'Hello World!'

    return h 

print(hash_table())
print(f'At "{list(hash_table().keys())[0]}" is "{hash_table()[list(hash_table().keys())[0]]}"')

c.send(b'1')
c.close()
