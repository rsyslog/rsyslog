# call this via "python[3] script name"
import argparse
import json
import os
import zlib
import base64

try:
    from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer # Python 2
except ImportError:
    from http.server import BaseHTTPRequestHandler, HTTPServer # Python 3

# Keep track of data received at each path
data = {}

metadata = {'posts': 0, 'fail_after': 0, 'fail_every': -1, 'decompress': False, 'userpwd': ''}


class MyHandler(BaseHTTPRequestHandler):
    """
    POST'd data is kept in the data global dict.
    Keys are the path, values are the raw received data.
    Two post requests to <host>:<port>/post/endpoint means data looks like...
        {"/post/endpoint": ["{\"msgnum\":\"00001\"}", "{\"msgnum\":\"00001\"}"]}

    GET requests return all data posted to that endpoint as a json list.
    Note that rsyslog usually sends escaped json data, so some parsing may be needed.
    A get request for <host>:<post>/post/endpoint responds with...
        ["{\"msgnum\":\"00001\"}", "{\"msgnum\":\"00001\"}"]
    """

    def validate_auth(self):
        # header format for basic authentication
        # 'Authorization: Basic <base 64 encoded uid:pwd>'
        if 'Authorization' not in self.headers:
            self.send_response(401)
            self.end_headers()
            self.wfile.write(b'missing "Authorization" header')
            return False

        auth_header = self.headers['Authorization']
        _, b64userpwd = auth_header.split()
        userpwd = base64.b64decode(b64userpwd).decode('utf-8')
        if userpwd != metadata['userpwd']:
            self.send_response(401)
            self.end_headers()
            self.wfile.write(b'invalid auth: {0}'.format(userpwd))
            return False

        return True

    def do_POST(self):
        metadata['posts'] += 1

        if metadata['userpwd']:
            if not self.validate_auth():
                return

        if metadata['fail_with_400_after'] != -1 and metadata['posts'] > metadata['fail_with_400_after']:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b'BAD REQUEST')
            return

        if metadata['posts'] > 1 and metadata['fail_every'] != -1 and metadata['posts'] % metadata['fail_every'] == 0:
            self.send_response(500)
            self.end_headers()
            self.wfile.write(b'INTERNAL ERROR')
            return

        content_length = int(self.headers['Content-Length'] or 0)
        raw_data = self.rfile.read(content_length)

        if metadata['decompress']:
            post_data = zlib.decompress(raw_data, 31)
        else:
            post_data = raw_data

        if self.path not in data:
            data[self.path] = []
        data[self.path].append(post_data.decode('utf-8'))

        res = json.dumps({'msg': 'ok'}).encode('utf8')

        self.send_response(200)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', len(res))
        self.end_headers()

        self.wfile.write(res)
        return

    def do_GET(self):
        if self.path in data:
            result = data[self.path]
        else:
            result = []

        res = json.dumps(result).encode('utf8')

        self.send_response(200)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', len(res))
        self.end_headers()

        self.wfile.write(res)
        return


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Archive and delete core app log files')
    parser.add_argument('-p', '--port', action='store', type=int, default=8080, help='port')
    parser.add_argument('--port-file', action='store', type=str, default='', help='file to store listen port number')
    parser.add_argument('-i', '--interface', action='store', type=str, default='localhost', help='port')
    parser.add_argument('--fail-after', action='store', type=int, default=0, help='start failing after n posts')
    parser.add_argument('--fail-every', action='store', type=int, default=-1, help='fail every n posts')
    parser.add_argument('--fail-with-400-after', action='store', type=int, default=-1, help='fail with 400 after n posts')
    parser.add_argument('--decompress', action='store_true', default=False, help='decompress posted data')
    parser.add_argument('--userpwd', action='store', default='', help='only accept this user:password combination')
    args = parser.parse_args()
    metadata['fail_after'] = args.fail_after
    metadata['fail_every'] = args.fail_every
    metadata['fail_with_400_after'] = args.fail_with_400_after
    metadata['decompress'] = args.decompress
    metadata['userpwd'] = args.userpwd
    server = HTTPServer((args.interface, args.port), MyHandler)
    lstn_port = server.server_address[1]
    pid = os.getpid()
    print('starting omhttp test server at {interface}:{port} with pid {pid}'
          .format(interface=args.interface, port=lstn_port, pid=pid))
    if args.port_file != '':
        f = open(args.port_file, "w")
        f.write(str(lstn_port))
        f.close()
    server.serve_forever()
