import sys
import hashlib

HOST, PORT = '127.0.0.1', 8080

def hash_table(): return {[hashlib.md5('0'.encode()).hexdigest()] : 'Hello World!'}
