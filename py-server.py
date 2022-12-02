from wsgiref.simple_server import make_server
from os.path import exists
import pickle

store = {} if not(exists('save.p')) else pickle.load(open('save.p', 'rb'))

def _set_headers(start_response):
    status = '200 OK'
    response_headers = [('Content-Type', 'text/html')]
    start_response(status, response_headers)

def app(environ, start_response):
    _set_headers(start_response) 

    key = environ['PATH_INFO'][1:]
    rtype = environ['REQUEST_METHOD'].upper()
    if rtype == 'GET':
        response_body = store[key].encode()
    elif rtype == 'PUT':
        try: size = int(environ.get('CONTENT_LENGTH', 0))
        except (ValueError): size = 0 
        data = environ['wsgi.input'].read(size).decode()
        store[key] = data
        response_body = b''
    elif rtype == 'DELETE':
        try: store.pop(key)
        except KeyError: pass
        response_body = b''

    return [response_body]

HOST, PORT = 'localhost', 8080
if __name__ == '__main__':
    server = make_server(HOST, PORT, app=app)
    print(f'http://{HOST}:{PORT}')
    try: server.serve_forever()
    except KeyboardInterrupt: pass

    pickle.dump(store, open('save.p', 'wb'))
