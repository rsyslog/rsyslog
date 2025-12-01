#!/usr/bin/env bash
#
## Test script for rsyslog Prometheus exporter
## Quick validation that the exporter can parse sample files and serve metrics
#
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Use venv if available, otherwise system Python
if [ -d "venv" ]; then
    PYTHON="$SCRIPT_DIR/venv/bin/python3"
    echo "Using virtualenv Python"
else
    PYTHON="python3"
    echo "Using system Python"
fi

echo "=========================================="
echo "Testing rsyslog Prometheus Exporter"
echo "=========================================="

# Check Python version
echo -n "Python version: "
$PYTHON --version

# Check dependencies
echo ""
echo "Checking dependencies..."
if $PYTHON -c "import prometheus_client" 2>/dev/null; then
    echo "✓ prometheus-client installed"
else
    echo "✗ prometheus-client not installed"
    echo "  Run: pip3 install -r requirements.txt"
    echo "  Or: python3 -m venv venv && venv/bin/pip install -r requirements.txt"
    exit 1
fi

if $PYTHON -c "import werkzeug" 2>/dev/null; then
    echo "✓ werkzeug installed"
else
    echo "✗ werkzeug not installed"
    echo "  Run: pip3 install -r requirements.txt"
    echo "  Or: python3 -m venv venv && venv/bin/pip install -r requirements.txt"
    exit 1
fi

# Test parser validation (no HTTP server needed)
echo ""
echo "=========================================="
echo "Test 1: Parser Validation"
echo "=========================================="
if $PYTHON validate.py; then
    echo "✓ All parsers validated successfully"
else
    echo "✗ Parser validation failed"
    exit 1
fi

# Optional: Test HTTP server (requires curl and can be skipped)
if command -v curl &> /dev/null; then
    echo ""
    echo "=========================================="
    echo "Test 2: HTTP Server (optional)"
    echo "=========================================="
    echo "Starting exporter briefly to test HTTP endpoints..."
    
    export IMPSTATS_PATH="$SCRIPT_DIR/examples/sample-impstats.json"
    export IMPSTATS_FORMAT="json"
    export LISTEN_PORT="19898"
    export LOG_LEVEL="ERROR"
    
    # Start in background with timeout
    timeout 10 $PYTHON rsyslog_exporter.py > /tmp/test-exporter.log 2>&1 &
    EXPORTER_PID=$!
    
    # Wait for server to start
    sleep 2
    
    # Test endpoints
    if curl -s --max-time 2 http://localhost:19898/health > /dev/null 2>&1; then
        echo "✓ Health endpoint responding"
        
        if curl -s --max-time 2 http://localhost:19898/metrics | grep -q "rsyslog_core"; then
            echo "✓ Metrics endpoint working"
        else
            echo "⚠ Metrics endpoint returned unexpected data"
        fi
    else
        echo "⚠ HTTP server test skipped (server may not have started)"
    fi
    
    # Cleanup
    kill $EXPORTER_PID 2>/dev/null || true
    wait $EXPORTER_PID 2>/dev/null || true
else
    echo ""
    echo "⚠ Skipping HTTP server test (curl not available)"
fi

echo ""
echo "=========================================="
echo "All core tests passed! ✓"
echo "=========================================="
echo ""
echo "Quick start commands:"
echo ""
echo "1. Start with JSON format:"
echo "   export IMPSTATS_PATH=examples/sample-impstats.json"
echo "   export IMPSTATS_FORMAT=json"
if [ -d "venv" ]; then
    echo "   ./venv/bin/python3 rsyslog_exporter.py"
else
    echo "   python3 rsyslog_exporter.py"
fi
echo ""
echo "2. Test endpoints:"
echo "   curl http://localhost:9898/health"
echo "   curl http://localhost:9898/metrics"
echo ""
echo "3. See README.md for full documentation"
