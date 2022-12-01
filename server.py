from http.server import HTTPServer, BaseHTTPRequestHandler
import pickle
from os.path import exists

store = {'0' : 'Hello World!'} if not(exists('save.p')) else pickle.load(open('save.p', 'rb'))

class Volume(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.end_headers()

        key = self.path[1:]
        self.wfile.write(store[key].encode())

HOST, PORT = 'localhost', 8080
if __name__ == '__main__':
    server = HTTPServer((HOST, PORT), Volume)
    print(f'http://{HOST}:{PORT}')

    try: server.serve_forever()
    except KeyboardInterrupt: pass

    pickle.dump(store, open('save.p', 'wb'))
    server.server_close()
