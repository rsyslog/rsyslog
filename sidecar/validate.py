#!/usr/bin/env python3
"""
Quick validation script to test parsing without starting HTTP server.
"""
import sys
sys.path.insert(0, '.')

from rsyslog_exporter import parse_json_impstats, parse_cee_impstats, parse_prometheus_impstats

def test_json_parsing():
    """Test JSON format parsing."""
    print("Testing JSON parsing...")
    metrics = parse_json_impstats("examples/sample-impstats.json")
    print(f"  ✓ Parsed {len(metrics)} metrics from JSON")
    
    # Verify some expected metrics
    metric_names = [m.name for m in metrics]
    assert any("rsyslog_core_utime" in n for n in metric_names), "Missing core metrics"
    assert any("rsyslog_core_action_processed" in n for n in metric_names), "Missing action metrics"
    print("  ✓ Found expected metric types")
    return True

def test_cee_parsing():
    """Test CEE format parsing."""
    print("Testing CEE parsing...")
    metrics = parse_cee_impstats("examples/sample-impstats.cee")
    print(f"  ✓ Parsed {len(metrics)} metrics from CEE")
    assert len(metrics) > 0, "No metrics parsed from CEE"
    return True

def test_prometheus_parsing():
    """Test Prometheus native format parsing."""
    print("Testing Prometheus parsing...")
    metrics = parse_prometheus_impstats("examples/sample-impstats.prom")
    print(f"  ✓ Parsed {len(metrics)} metrics from Prometheus format")
    
    # Check that we got the expected metrics
    metric_names = [m.name for m in metrics]
    assert "rsyslog_global_utime" in metric_names, "Missing global utime"
    assert "rsyslog_action_processed" in metric_names, "Missing action processed"
    print("  ✓ Found expected metrics")
    return True

def main():
    print("=" * 60)
    print("rsyslog Prometheus Exporter - Parser Validation")
    print("=" * 60)
    print()
    
    try:
        test_json_parsing()
        print()
        test_cee_parsing()
        print()
        test_prometheus_parsing()
        print()
        print("=" * 60)
        print("All parser tests passed! ✓")
        print("=" * 60)
        return 0
    except Exception as e:
        print()
        print(f"✗ Test failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
