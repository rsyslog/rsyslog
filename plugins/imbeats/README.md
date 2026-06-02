# imbeats Notes

`imbeats` is a dedicated Beats/Lumberjack v2 input module built on rsyslog's
`netstrm` transport subsystem instead of `tcpsrv`.

## Deferred Representation Decision

The first implementation maps decoded Beat event fields into the top-level
structured message tree `$!` and keeps the original JSON payload in `msg`.
Transport and protocol metadata is stored under `$!metadata!imbeats`.

This shape is intentional for the common pipeline:

- receive Beat events in rsyslog
- process or route in rsyslog
- finally emit JSON to Elasticsearch with minimal reshaping

This is **not** the only valid representation. A more rsyslog-native split such
as `msg + $!beats + $!metadata!imbeats` remains a reasonable alternative.

The long-term default and any user-selectable representation mode need to be
decided later based on real usage and operator feedback.
