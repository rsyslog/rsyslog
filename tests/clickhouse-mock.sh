# Helper functions for omclickhouse tests that rely on the local mock server.

clickhouse_mock_setup() {
    if [ -z "$PYTHON" ]; then
        echo "TESTBENCH ERROR: PYTHON variable is unset"
        error_exit 100
    fi

    export CLICKHOUSE_MOCK_BASE="${RSYSLOG_DYNNAME}.clickhouse_mock"
    export CLICKHOUSE_MOCK_SINK_DIR="${CLICKHOUSE_MOCK_BASE}/requests"
    export CLICKHOUSE_MOCK_READY_FILE="${CLICKHOUSE_MOCK_BASE}/ready"
    mkdir -p "$CLICKHOUSE_MOCK_SINK_DIR"

    export CLICKHOUSE_MOCK_PORT="$(get_free_port)"

    "$PYTHON" "${srcdir}/clickhouse-mock-server.py" \
        --host 127.0.0.1 \
        --port "$CLICKHOUSE_MOCK_PORT" \
        --sink-dir "$CLICKHOUSE_MOCK_SINK_DIR" \
        --ready-file "$CLICKHOUSE_MOCK_READY_FILE" &
    CLICKHOUSE_MOCK_PID=$!
    wait_file_exists "$CLICKHOUSE_MOCK_READY_FILE"
    trap 'clickhouse_mock_teardown' EXIT
}

clickhouse_mock_teardown() {
    if [ -n "$CLICKHOUSE_MOCK_PID" ]; then
        kill "$CLICKHOUSE_MOCK_PID" 2>/dev/null || true
        wait "$CLICKHOUSE_MOCK_PID" 2>/dev/null
        unset CLICKHOUSE_MOCK_PID
    fi
}

clickhouse_mock_request_file() {
    printf '%s/request_%04d.sql' "$CLICKHOUSE_MOCK_SINK_DIR" "$1"
}

clickhouse_mock_request_count() {
    ls -1 "$CLICKHOUSE_MOCK_SINK_DIR"/request_*.sql 2>/dev/null | wc -l | tr -d ' '
}
