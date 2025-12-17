#!/usr/bin/env python3
"""
Simple HTTP proxy server for omotel proxy testing.

This proxy server:
- Forwards HTTP requests to a target server
- Supports basic authentication
- Logs proxy usage for verification
- Writes received data to a file for inspection
"""
import argparse
import base64
import json
import os
import sys
import urllib.parse
import urllib.request

try:
    from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer  # Python 2
    from SocketServer import ThreadingMixIn
except ImportError:
    from http.server import BaseHTTPRequestHandler, HTTPServer  # Python 3
    from socketserver import ThreadingMixIn

# Global state
proxy_metadata = {
    'requests': [],
    'target_host': '127.0.0.1',
    'target_port': 4318,
    'require_auth': False,
    'expected_user': '',
    'expected_password': '',
    'data_file': None
}


class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):
    """Threading HTTP server to handle concurrent requests."""
    pass


class ProxyHandler(BaseHTTPRequestHandler):
    """HTTP proxy handler that forwards requests to target server."""

    def validate_proxy_auth(self):
        """Validate Proxy-Authorization header if required."""
        if not proxy_metadata['require_auth']:
            return True

        if 'Proxy-Authorization' not in self.headers:
            self.send_response(407)  # Proxy Authentication Required
            self.send_header('Proxy-Authenticate', 'Basic realm="Proxy"')
            self.end_headers()
            self.wfile.write(b'Proxy authentication required')
            return False

        auth_header = self.headers['Proxy-Authorization']
        if not auth_header.startswith('Basic '):
            self.send_response(407)
            self.send_header('Proxy-Authenticate', 'Basic realm="Proxy"')
            self.end_headers()
            self.wfile.write(b'Invalid proxy authentication method')
            return False

        try:
            _, b64userpwd = auth_header.split(' ', 1)
            userpwd = base64.b64decode(b64userpwd).decode('utf-8')
            user, password = userpwd.split(':', 1)
            expected_user = proxy_metadata['expected_user']
            expected_password = proxy_metadata['expected_password']

            if user != expected_user or password != expected_password:
                self.send_response(407)
                self.send_header('Proxy-Authenticate', 'Basic realm="Proxy"')
                self.end_headers()
                self.wfile.write(b'Invalid proxy credentials')
                return False
        except Exception as e:
            self.send_response(407)
            self.send_header('Proxy-Authenticate', 'Basic realm="Proxy"')
            self.end_headers()
            self.wfile.write(f'Proxy auth error: {e}'.encode('utf-8'))
            return False

        return True

    def log_message(self, format, *args):
        """Override to reduce noise in test output."""
        # Only log errors
        if 'error' in format.lower() or args[1].startswith('4') or args[1].startswith('5'):
            super().log_message(format, *args)

    def do_CONNECT(self):
        """Handle CONNECT method for HTTPS tunneling."""
        if not self.validate_proxy_auth():
            return

        # For HTTPS, we'd need to implement SSL tunneling
        # For simplicity in tests, we'll just return an error
        # Most tests will use HTTP, not HTTPS through proxy
        self.send_response(501)
        self.end_headers()
        self.wfile.write(b'CONNECT method not fully implemented (use HTTP for testing)')

    def do_POST(self):
        """Handle POST requests by forwarding to target server."""
        if not self.validate_proxy_auth():
            return

        # Record this request
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length) if content_length > 0 else b''

        # When using HTTP proxy, libcurl sends the full URL in the path
        # e.g., "http://127.0.0.1:50101/v1/logs" instead of just "/v1/logs"
        # Parse the path to extract the actual target path
        target_path = self.path
        if self.path.startswith('http://') or self.path.startswith('https://'):
            # Full URL provided - parse it
            try:
                parsed = urllib.parse.urlparse(self.path)
                target_path = parsed.path
                if parsed.query:
                    target_path += '?' + parsed.query
            except Exception:
                # If parsing fails, use the path as-is
                pass

        request_info = {
            'method': 'POST',
            'path': self.path,
            'target_path': target_path,
            'headers': dict(self.headers),
            'body': body.decode('utf-8', errors='replace'),
            'client': self.client_address[0]
        }
        proxy_metadata['requests'].append(request_info)

        # Forward request to target server
        target_url = f"http://{proxy_metadata['target_host']}:{proxy_metadata['target_port']}{target_path}"

        try:
            # Create request with original headers (except Host and Connection)
            # When forwarding through proxy, we need to set the Host header to the target
            req_headers = {}
            for header, value in self.headers.items():
                header_lower = header.lower()
                if header_lower not in ('host', 'connection', 'proxy-authorization'):
                    req_headers[header] = value
            
            # Set Host header to target server
            req_headers['Host'] = f"{proxy_metadata['target_host']}:{proxy_metadata['target_port']}"

            req = urllib.request.Request(target_url, data=body, headers=req_headers, method='POST')

            with urllib.request.urlopen(req, timeout=10) as response:
                # Forward response
                self.send_response(response.getcode())
                for header, value in response.headers.items():
                    if header.lower() not in ('connection', 'transfer-encoding'):
                        self.send_header(header, value)
                self.end_headers()

                response_body = response.read()
                self.wfile.write(response_body)

        except urllib.error.HTTPError as e:
            # Forward error response
            self.send_response(e.code)
            for header, value in e.headers.items():
                if header.lower() not in ('connection', 'transfer-encoding'):
                    self.send_header(header, value)
            self.end_headers()
            error_body = e.read()
            self.wfile.write(error_body)

        except Exception as e:
            # Log the error for debugging
            import traceback
            error_msg = f'Proxy error: {e}\n{traceback.format_exc()}'
            print(f"Proxy error: {error_msg}", file=sys.stderr)
            self.send_response(502)  # Bad Gateway
            self.end_headers()
            self.wfile.write(f'Proxy error: {e}'.encode('utf-8'))

    def do_GET(self):
        """Handle GET requests (for health checks, data retrieval)."""
        if self.path == '/proxy/requests':
            # Return list of proxied requests
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            response = json.dumps(proxy_metadata['requests'], indent=2)
            self.wfile.write(response.encode('utf-8'))
        elif self.path == '/proxy/clear':
            # Clear request log
            proxy_metadata['requests'] = []
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b'OK')
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'Not found')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Simple HTTP proxy for omotel testing')
    parser.add_argument('-p', '--port', type=int, default=0, help='Listen port (0 = auto)')
    parser.add_argument('--port-file', type=str, default='', help='File to write listen port')
    parser.add_argument('--target-host', type=str, default='127.0.0.1', help='Target server host')
    parser.add_argument('--target-port', type=int, default=4318, help='Target server port')
    parser.add_argument('--require-auth', action='store_true', help='Require proxy authentication')
    parser.add_argument('--user', type=str, default='', help='Expected proxy username')
    parser.add_argument('--password', type=str, default='', help='Expected proxy password')
    parser.add_argument('--data-file', type=str, default='', help='File to save request data')
    args = parser.parse_args()

    proxy_metadata['target_host'] = args.target_host
    proxy_metadata['target_port'] = args.target_port
    proxy_metadata['require_auth'] = args.require_auth
    proxy_metadata['expected_user'] = args.user
    proxy_metadata['expected_password'] = args.password
    proxy_metadata['data_file'] = args.data_file

    server = ThreadingHTTPServer(('127.0.0.1', args.port), ProxyHandler)
    listen_port = server.server_address[1]
    pid = os.getpid()

    print(f'Starting proxy server on 127.0.0.1:{listen_port} (pid {pid})')
    print(f'Forwarding to {args.target_host}:{args.target_port}')
    if args.require_auth:
        print(f'Proxy authentication required: {args.user}:***')

    if args.port_file:
        with open(args.port_file, 'w') as f:
            f.write(str(listen_port))

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('\nShutting down proxy server...')
        server.shutdown()
        # Save request data if requested
        if args.data_file and proxy_metadata['requests']:
            with open(args.data_file, 'w') as f:
                json.dump(proxy_metadata['requests'], f, indent=2)

