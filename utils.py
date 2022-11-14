import hashlib

def hash_table():
    h = {}
    h[hashlib.md5('0'.encode()).hexdigest()] = 'Hello World'
    return h
