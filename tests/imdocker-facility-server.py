#!/usr/bin/env python
# SPDX-License-Identifier: Apache-2.0
"""Minimal loopback Docker API fixture; never contacts a Docker daemon."""
from __future__ import print_function
import io
import json
import os
import struct
import sys
try:
    from http.server import BaseHTTPRequestHandler, HTTPServer
except ImportError:
    from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

CONTAINER_ID = 'a' * 64


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        endpoint = self.path.split('?', 1)[0]
        if endpoint == '/v1.27/containers/json':
            body = json.dumps([{'Id': CONTAINER_ID, 'Names': ['/facility-fixture'],
                                'Image': 'fixture', 'ImageID': 'fixture',
                                'Created': 1, 'Labels': {}}]).encode('ascii')
            content_type = 'application/json'
        elif endpoint == '/v1.27/containers/' + CONTAINER_ID + '/logs':
            payload = b'facility-marker\n'
            body = struct.pack('>BxxxI', 1, len(payload)) + payload
            content_type = 'application/vnd.docker.raw-stream'
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)
        self.wfile.flush()


server = HTTPServer(('127.0.0.1', 0), Handler)
with io.open(sys.argv[1] + '.tmp', 'w', encoding='ascii') as output:
    output.write(u'{}'.format(server.server_address[1]))
os.rename(sys.argv[1] + '.tmp', sys.argv[1])
server.serve_forever()
