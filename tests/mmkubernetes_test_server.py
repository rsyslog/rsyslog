# Used by the mmkubernetes tests
# This is a simple http server which responds to kubernetes api requests
# and responds with kubernetes api server responses
# added 2018-04-06 by richm, released under ASL 2.0
import os
import json
import sys

try:
    from http.server import HTTPServer, BaseHTTPRequestHandler
except ImportError:
    from BaseHTTPServer import HTTPServer, BaseHTTPRequestHandler

ns_template = '''{{
  "kind": "Namespace",
  "apiVersion": "v1",
  "metadata": {{
    "name": "{namespace_name}",
    "selfLink": "/api/v1/namespaces/{namespace_name}",
    "uid": "{namespace_name}-id",
    "resourceVersion": "2988",
    "creationTimestamp": "2018-04-09T21:56:39Z",
    "labels": {{
      "label.1.key":"label 1 value",
      "label.2.key":"label 2 value",
      "label.with.empty.value":""
    }},
    "annotations": {{
      "k8s.io/description": "",
      "k8s.io/display-name": "",
      "k8s.io/node-selector": "",
      "k8s.io/sa.scc.mcs": "s0:c9,c4",
      "k8s.io/sa.scc.supplemental-groups": "1000080000/10000",
      "k8s.io/sa.scc.uid-range": "1000080000/10000",
      "quota.k8s.io/cluster-resource-override-enabled": "false"
    }}
  }},
  "spec": {{
    "finalizers": [
      "openshift.io/origin",
      "kubernetes"
    ]
  }},
  "status": {{
    "phase": "Active"
  }}
}}'''

pod_template = '''{{
  "kind": "Pod",
  "apiVersion": "v1",
  "metadata": {{
    "name": "{pod_name}",
    "generateName": "{pod_name}-prefix",
    "namespace": "{namespace_name}",
    "selfLink": "/api/v1/namespaces/{namespace_name}/pods/{pod_name}",
    "uid": "{pod_name}-id",
    "resourceVersion": "3486",
    "creationTimestamp": "2018-04-09T21:56:39Z",
    "labels": {{
      "component": "{pod_name}-component",
      "deployment": "{pod_name}-deployment",
      "deploymentconfig": "{pod_name}-dc",
      "custom.label": "{pod_name}-label-value",
      "label.with.empty.value":""
    }},
    "annotations": {{
      "k8s.io/deployment-config.latest-version": "1",
      "k8s.io/deployment-config.name": "{pod_name}-dc",
      "k8s.io/deployment.name": "{pod_name}-deployment",
      "k8s.io/custom.name": "custom value",
      "annotation.with.empty.value":""
    }}
  }},
{spec_block}  "status": {{
    "phase": "Running",
    "hostIP": "172.18.4.32",
    "podIP": "10.128.0.14",
    "startTime": "2018-04-09T21:57:39Z"
  }}
}}'''

err_template = '''{{
  "kind": "Status",
  "apiVersion": "v1",
  "metadata": {{

  }},
  "status": "Failure",
  "message": "{kind} \\\"{objectname}\\\" {err}",
  "reason": "{reason}",
  "details": {{
    "name": "{objectname}",
    "kind": "{kind}"
  }},
  "code": {code}
}}'''

is_busy = False
include_node_name = os.environ.get("MMK8S_INCLUDE_NODE_NAME") == "1"
# Optional path to a file holding the currently-valid bearer token. When set
# (via MMK8S_EXPECTED_TOKEN_FILE), the server rejects any request whose
# "Authorization: Bearer <tok>" does not match the file's current contents with
# HTTP 401. The test rotates this file to exercise the token reload-on-401 path
# in mmkubernetes. When unset, no auth check is performed and existing tests
# behave as before.
expected_token_file = os.environ.get("MMK8S_EXPECTED_TOKEN_FILE")


def read_expected_token():
    """Read the currently-valid token from expected_token_file, or None."""
    if not expected_token_file:
        return None
    try:
        with open(expected_token_file) as ff:
            return ff.read().strip()
    except IOError:
        return None


class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        # "http://localhost:18443/api/v1/namespaces/namespace-name2"
        # parse url - either /api/v1/namespaces/$ns_name
        # or
        # /api/v1/namespaces/$ns_name/pods/$pod_name
        global is_busy
        # if token checking is enabled, reject a stale/missing bearer token with
        # 401 so the module reloads the token from disk and retries
        want_token = read_expected_token()
        if want_token is not None:
            auth = self.headers.get('Authorization', '')
            got_token = auth[len('Bearer '):].strip() if auth.startswith('Bearer ') else ''
            if got_token != want_token:
                resp = err_template.format(kind='pods', objectname=self.path,
                                           err='invalid bearer token', reason='Unauthorized', code=401)
                self.log_error(resp)
                self.send_response(401)
                self.end_headers()
                self.wfile.write(json.dumps(json.loads(resp), separators=(',', ':')).encode())
                return
        comps = self.path.split('/')
        status = 400
        if len(comps) >= 5 and comps[1] == 'api' and comps[2] == 'v1' and comps[3] == 'namespaces':
            resp = None
            hsh = {'namespace_name': comps[4], 'objectname': comps[4], 'kind': 'namespace'}
            if len(comps) == 5:  # namespace
                resp_template = ns_template
                status = 200
            elif len(comps) == 7 and comps[5] == 'pods':  # pod
                hsh['pod_name'] = comps[6]
                hsh['kind'] = 'pods'
                hsh['objectname'] = hsh['pod_name']
                if include_node_name:
                    hsh['spec_block'] = (
                        '  "spec": {{\n'
                        '    "nodeName": "{pod_name}-node"\n'
                        '  }},\n'
                    ).format(**hsh)
                else:
                    hsh['spec_block'] = ''
                resp_template = pod_template
                status = 200
            else:
                resp = '{{"error":"do not recognize {0}"}}'.format(self.path)
            if hsh['objectname'].endswith('not-found'):
                status = 404
                hsh['reason'] = 'NotFound'
                hsh['err'] = 'not found'
                resp_template = err_template
            elif hsh['objectname'].endswith('busy'):
                is_busy = not is_busy
                if is_busy:
                    status = 429
                    hsh['reason'] = 'Busy'
                    hsh['err'] = 'server is too busy'
                    resp_template = err_template
            elif hsh['objectname'].endswith('error'):
                status = 500
                hsh['reason'] = 'Error'
                hsh['err'] = 'server is failing'
                resp_template = err_template
            if not resp:
                hsh['code'] = status
                resp = resp_template.format(**hsh)
        else:
            resp = '{{"error":"do not recognize {0}"}}'.format(self.path)
        if not status == 200:
            self.log_error(resp)
        self.send_response(status)
        self.end_headers()
        self.wfile.write(json.dumps(json.loads(resp), separators=(',', ':')).encode())


port = int(sys.argv[1])
port_file = sys.argv[4] if len(sys.argv) > 4 else ""

httpd = HTTPServer(('localhost', port), SimpleHTTPRequestHandler)
listen_port = httpd.server_address[1]

if port_file:
    with open(port_file, "w") as ff:
        ff.write('{0}\n'.format(listen_port))

# write "started" to file named in argv[3]
with open(sys.argv[3], "w") as ff:
    ff.write("started\n")

# write pid to file named in argv[2]
with open(sys.argv[2], "w") as ff:
    ff.write('{0}\n'.format(os.getpid()))

httpd.serve_forever()
