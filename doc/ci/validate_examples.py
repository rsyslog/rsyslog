#!/usr/bin/env python3
"""
Validate rsyslog configuration examples.

This script iterates through all .conf files in doc/examples/ and validates
them using rsyslogd -N1 (syntax check only).
"""

import os
import subprocess
import sys
from pathlib import Path

def validate_config(config_file):
    """Validate a single rsyslog configuration file."""
    cmd = ['rsyslogd', '-N1', '-f', str(config_file)]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            return True, "Valid"
        else:
            return False, result.stderr
    except FileNotFoundError:
        return False, "rsyslogd not found in PATH"
    except Exception as e:
        return False, str(e)

def main():
    """Main function to validate all example configurations."""
    # Find the examples directory
    script_dir = Path(__file__).parent
    examples_dir = script_dir.parent / 'examples'
    
    if not examples_dir.exists():
        print(f"Error: Examples directory not found at {examples_dir}")
        sys.exit(1)
    
    # Find all .conf files
    conf_files = list(examples_dir.glob('*.conf'))
    
    if not conf_files:
        print(f"No .conf files found in {examples_dir}")
        sys.exit(0)
    
    print(f"Validating {len(conf_files)} configuration files in {examples_dir}")
    print("=" * 70)
    
    failed = 0
    for conf_file in conf_files:
        valid, message = validate_config(conf_file)
        
        if valid:
            print(f"✓ {conf_file.name}: {message}")
        else:
            print(f"✗ {conf_file.name}: {message}")
            failed += 1
    
    print("=" * 70)
    print(f"Summary: {len(conf_files) - failed} passed, {failed} failed")
    
    if failed > 0:
        sys.exit(1)

if __name__ == '__main__':
    main()