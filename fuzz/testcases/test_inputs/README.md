# Test Input Files for rsyslog Fuzzing

This directory contains various input files designed to test different aspects of the rsyslog parser and achieve better code coverage.

## File Descriptions

- `rfc3164_standard.txt`: Standard RFC 3164 formatted syslog message with valid structure
- `rfc5424_structured.txt`: RFC 5424 formatted message with structured data (SD)
- `rfc3164_minimal.txt`: Minimal RFC 3164 message with just required fields
- `rfc3164_large_pri.txt`: RFC 3164 message with an abnormally large priority value
- `malformed_priority.txt`: Syslog message with malformed priority field (missing closing bracket)
- `no_priority.txt`: Syslog message with no priority field at all
- `very_long_message.txt`: Very long message to test length limits and processing
- `malformed_structured_data.txt`: RFC 5424 message with malformed structured data section
- `rfc5424_no_structured.txt`: RFC 5424 message with no structured data (uses '-' as placeholder)
- `empty_message.txt`: Message with an empty payload after the syslog header
- `empty_test.txt`: Zero-byte testcase for dispatcher and parser lifecycle coverage
- `binary_data.txt`: File containing all possible ASCII values (0-127) as text
- `binary_data_bytes.txt`: File containing all possible byte values (0-255) as binary data
- `malformed_timestamp.txt`: Message with completely invalid timestamp format
- `multiple_messages.txt`: File containing multiple syslog messages in one file
- `special_chars.txt`: Message containing special characters and escape sequences
- `control_chars_actual.txt`: Message containing actual control characters (bytes 0x01-0x0F)
- `null_bytes.txt`: Message containing null bytes to test string parsing robustness
- `ipv6_host.txt`: Message with IPv6 address in hostname field
- `unix_timestamp.txt`: Message with Unix timestamp format (though not standard)
- `buffer_overflow_attempt.txt`: Very large message to test buffer limits
- `octet_counted_rfc5424.txt`: RFC 5424 message framed with RFC6587 octet-counting prefix for TCP inputs
- `cee_json_event.txt`: CEE-prefixed JSON payload to drive JSON accessors and structured data markers
- `structured_data_escape.txt`: Structured data block exercising escaped quotes/brackets and empty parameters

The `short_timestamp_*.txt` inputs keep timestamp formatting exercised with
buffers shorter than its full field set. `legacy_queue_filename.txt` selects
the owning-string legacy configuration handler and is used by the same-process
`A -> B -> A` smoke sequence to guard iteration cleanup.
