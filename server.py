from http.server import HTTPServer, BaseHTTPRequestHandler

hash_table = {'0' : 'Hello World!'}

class Volume(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.end_headers()

        key = self.path[1:]
        self.wfile.write(hash_table[key].encode())
        
HOST, PORT = 'localhost', 8080
if __name__ == '__main__':
    server = HTTPServer((HOST, PORT), Volume)
    print(f'http://{HOST}:{PORT}')

    try: server.serve_forever()
    except KeyboardInterrupt: pass
    server.server_close()
