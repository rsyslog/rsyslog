#!/bin/bash
## Validate custom pattern loading and section detection for mmsnareparse.
unset RSYSLOG_DYNNAME
. ${srcdir:=.}/diag.sh init

DEF_FILE="${RSYSLOG_DYNNAME}.defs.json"
cat >"$DEF_FILE" <<'JSON'
{
  "sections": [
    {
      "pattern": "Custom Block*",
      "canonical": "CustomBlock",
      "behavior": "standard",
      "priority": 250,
      "sensitivity": "case_insensitive"
    }
  ],
  "fields": [
    {
      "pattern": "CustomEventTag",
      "canonical": "CustomEventTag",
      "section": "EventData",
      "priority": 80,
      "value_type": "string"
    }
  ],
  "eventFields": [
    {
      "event_id": 4001,
      "patterns": [
        {
          "pattern": "WidgetID",
          "canonical": "WidgetID",
          "section": "CustomBlock",
          "value_type": "string"
        }
      ]
    }
  ],
  "events": [
    {
      "event_id": 4001,
      "category": "Custom",
      "subtype": "Injected",
      "outcome": "success"
    }
  ]
}
JSON

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmsnareparse/.libs/mmsnareparse" \
       definition.file="'${PWD}/${DEF_FILE}'" \
       validation.mode="strict")

template(name="customfmt" type="list") {
    property(name="$!win!Event!Category")
    constant(value=",")
    property(name="$!win!CustomBlock!WidgetID")
    constant(value=",")
    property(name="$!win!EventData!CustomEventTag")
    constant(value=",")
    property(name="$!win!Event!Outcome")
    constant(value="\n")
}

action(type="mmsnareparse")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="customfmt")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
'

startup

assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
tcpflood -m 1 -I "${srcdir}/testsuites/mmsnareparse/sample-custom-pattern.data"

shutdown_when_empty
wait_shutdown

content_check ',ZX-42,Demo,success' "$RSYSLOG_OUT_LOG"

rm -f "$DEF_FILE"

exit_test
