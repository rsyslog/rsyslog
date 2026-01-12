#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
JSON semantic equality validator for rsyslog tests.
Compares input JSON (after cookie removal) with output JSON for semantic equality.
Ignores whitespace differences and field ordering.
"""
from __future__ import print_function
import json
import sys

def extract_json_from_syslog(line, cookie="<01> @cee:"):
    """Extract JSON part from syslog line with cookie."""
    if cookie in line:
        json_part = line.split(cookie, 1)[1].strip()
        return json_part
    return line.strip()


def normalize_json(obj):
    """Recursively normalize JSON object for comparison."""
    if isinstance(obj, dict):
        # Sort keys for consistent comparison
        return {k: normalize_json(v) for k, v in sorted(obj.items())}
    elif isinstance(obj, list):
        return [normalize_json(item) for item in obj]
    else:
        return obj


def compare_json_semantically(json1_str, json2_str):
    """Compare two JSON strings semantically."""
    try:
        obj1 = json.loads(json1_str)
        obj2 = json.loads(json2_str)
        
        norm1 = normalize_json(obj1)
        norm2 = normalize_json(obj2)
        
        if norm1 == norm2:
            return True, "JSON objects are semantically equal"
        else:
            return False, "JSON objects differ:\nExpected: {0}\nActual: {1}".format(
                json.dumps(norm1, sort_keys=True, indent=2),
                json.dumps(norm2, sort_keys=True, indent=2)
            )
    
    except ValueError as e:
        return False, "JSON parsing error: {0}".format(e)
    except Exception as e:
        return False, "JSON parsing error: {0}".format(e)


def validate_files(input_file, output_file, cookie="<01> @cee:"):
    """Validate that input and output files contain semantically equal JSON."""
    try:
        with open(input_file, 'r') as f:
            input_lines = f.readlines()
        
        with open(output_file, 'r') as f:
            output_lines = f.readlines()
        
        if len(input_lines) != len(output_lines):
            print("ERROR: Line count mismatch. Input: {0}, Output: {1}".format(len(input_lines), len(output_lines)))
            return False
        
        all_passed = True
        for i, (input_line, output_line) in enumerate(zip(input_lines, output_lines), 1):
            # Extract JSON from input (remove syslog header and cookie)
            input_json = extract_json_from_syslog(input_line, cookie)
            output_json = output_line.strip()
            
            # Skip empty lines
            if not input_json and not output_json:
                continue
            
            success, message = compare_json_semantically(input_json, output_json)
            if success:
                print("Line {0}: ✓ JSON semantically equal".format(i))
            else:
                print("Line {0}: ✗ {1}".format(i, message))
                all_passed = False
        
        return all_passed
    
    except IOError as e:
        print("ERROR: File not found: {0}".format(e))
        return False
    except Exception as e:
        print("ERROR: {0}".format(e))
        return False


def main():
    if len(sys.argv) < 3:
        print("Usage: validate_json_equality.py <input_file> <output_file> [cookie]")
        print("Example: validate_json_equality.py testsuites/qradar_json rsyslog.out.log '<01> @cee:'")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    cookie = sys.argv[3] if len(sys.argv) > 3 else "<01> @cee:"
    
    print("Validating JSON equality between {0} and {1}".format(input_file, output_file))
    print("Using cookie: {0}".format(cookie))
    print("-" * 50)
    
    success = validate_files(input_file, output_file, cookie)
    
    if success:
        print("-" * 50)
        print("✓ All JSON comparisons passed! Input and output are semantically equal.")
        sys.exit(0)
    else:
        print("-" * 50)
        print("✗ JSON validation failed!")
        sys.exit(1)


if __name__ == "__main__":
    main()
