import requests
import sys

try: print(requests.get(f'http://{sys.argv[1]}').text)
except: pass
