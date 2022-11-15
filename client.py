import requests
import sys

try: print(requests.get(f'http://localhost:8080{sys.argv[1]}').text)
except: pass
