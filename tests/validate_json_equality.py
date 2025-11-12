#!/usr/bin/env python3
"""
JSON semantic equality validator for rsyslog tests.
Compares input JSON (after cookie removal) with output JSON for semantic equality.
Ignores whitespace differences and field ordering.
"""

import json
import sys
from typing import Any


def extract_json_from_syslog(line: str, cookie: str = "<01> @cee:") -> str:
    """Extract JSON part from syslog line with cookie."""
    if cookie in line:
        json_part = line.split(cookie, 1)[1].strip()
        return json_part
    return line.strip()


def normalize_json(obj: Any) -> Any:
    """Recursively normalize JSON object for comparison."""
    if isinstance(obj, dict):
        # Sort keys for consistent comparison
        return {k: normalize_json(v) for k, v in sorted(obj.items())}
    elif isinstance(obj, list):
        return [normalize_json(item) for item in obj]
    else:
        return obj


def compare_json_semantically(json1_str: str, json2_str: str):
    """Compare two JSON strings semantically."""
    try:
        obj1 = json.loads(json1_str)
        obj2 = json.loads(json2_str)
        
        norm1 = normalize_json(obj1)
        norm2 = normalize_json(obj2)
        
        if norm1 == norm2:
            return True, "JSON objects are semantically equal"
        else:
            return False, f"JSON objects differ:\nExpected: {json.dumps(norm1, sort_keys=True, indent=2)}\nActual: {json.dumps(norm2, sort_keys=True, indent=2)}"
    
    except json.JSONDecodeError as e:
        return False, f"JSON parsing error: {e}"


def validate_files(input_file: str, output_file: str, cookie: str = "<01> @cee:") -> bool:
    """Validate that input and output files contain semantically equal JSON."""
    try:
        with open(input_file, 'r') as f:
            input_lines = f.readlines()
        
        with open(output_file, 'r') as f:
            output_lines = f.readlines()
        
        if len(input_lines) != len(output_lines):
            print(f"ERROR: Line count mismatch. Input: {len(input_lines)}, Output: {len(output_lines)}")
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
                print(f"Line {i}: ✓ JSON semantically equal")
            else:
                print(f"Line {i}: ✗ {message}")
                all_passed = False
        
        return all_passed
    
    except FileNotFoundError as e:
        print(f"ERROR: File not found: {e}")
        return False
    except Exception as e:
        print(f"ERROR: {e}")
        return False


def main():
    if len(sys.argv) < 3:
        print("Usage: validate_json_equality.py <input_file> <output_file> [cookie]")
        print("Example: validate_json_equality.py testsuites/qradar_json rsyslog.out.log '<01> @cee:'")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    cookie = sys.argv[3] if len(sys.argv) > 3 else "<01> @cee:"
    
    print(f"Validating JSON equality between {input_file} and {output_file}")
    print(f"Using cookie: {cookie}")
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