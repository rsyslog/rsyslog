#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init

# --- ElasticSearch test parameters ---
export ES_PORT=19200
export NUMMESSAGES=1000    # slow test, thus low number - large number is NOT necessary

ensure_elasticsearch_ready
init_elasticsearch

# Create index with ES 7+ typeless mapping (no legacy type wrapper)
http_code=$(curl -sS -o /tmp/es_mapping_resp.json -w "%{http_code}" \
  -H 'Content-Type: application/json' \
  -X PUT "http://localhost:${ES_PORT}/rsyslog_testbench" \
  -d '{
        "mappings": {
          "properties": {
            "msgnum": { "type": "integer" }
          }
        }
      }')

# Accept 200 (updated) or 201 (created); otherwise fail fast so test result is meaningful
if [ "$http_code" != "200" ] && [ "$http_code" != "201" ]; then
  echo "ERROR: failed to create mapping (HTTP $http_code). Response:"
  cat /tmp/es_mapping_resp.json
  error_exit 1
fi

generate_conf
add_conf "
# Note: we must mess up with the template, because we can not
# instruct ES to put further constraints on the data type and
# values. So we require integer and make sure it is none.
template(name=\"tpl\" type=\"string\"
         string=\"{\\\"msgnum\\\":\\\"x%msg:F,58:2%\\\"}\")

module(load=\"../plugins/omelasticsearch/.libs/omelasticsearch\")
:msg, contains, \"msgnum:\" action(
  type=\"omelasticsearch\"
  template=\"tpl\"
  searchIndex=\"rsyslog_testbench\"
  searchType=\"_doc\"            # ES 7+: use _doc (or drop this param if module build supports typeless)
  serverport=\"${ES_PORT}\"
  bulkmode=\"off\"
  errorFile=\"./${RSYSLOG_DYNNAME}.errorfile\"
)
"

startup
injectmsg
shutdown_when_empty
wait_shutdown

if [ ! -f "${RSYSLOG_DYNNAME}.errorfile" ]; then
  echo "error: error file does not exist!"
  error_exit 1
fi

exit_test

