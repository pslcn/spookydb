from wsgiref.simple_server import make_server

class Middleware:
    def __init__(self, app):
        self.app = app

    def __call__(self, environ, start_response, *args, **kwargs):
        response = self.app(environ, start_response)
        return response

def app(environ, start_response):
    response_body = '\n'.join([f'{key}: {value}' for key, value in sorted(environ.items())])
    start_response(status='200 OK', headers=[('Content-Type', 'text/plain')]) 
    return [response_body.encode()]

server = make_server('localhost', 8080, app=app)
server.serve_forever()
