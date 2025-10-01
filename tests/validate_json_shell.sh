#!/bin/bash
# JSON equality validator using shell tools as fallback
# This script provides a simpler validation when Python is not available

validate_json_with_jq() {
    local input_file="$1"
    local output_file="$2" 
    local cookie="$3"
    
    echo "Using jq for JSON validation..."
    
    local line_num=1
    local all_passed=true
    
    while IFS= read -r input_line && IFS= read -r output_line <&3; do
        # Extract JSON from input (remove cookie)
        local input_json="${input_line#*$cookie}"
        input_json="${input_json# }" # remove leading space
        
        # Skip empty lines
        if [ -z "$input_json" ] && [ -z "$output_line" ]; then
            continue
        fi
        
        # Validate both are valid JSON and compare
        local input_normalized
        local output_normalized
        
        if input_normalized=$(echo "$input_json" | jq -S -c . 2>/dev/null) && \
           output_normalized=$(echo "$output_line" | jq -S -c . 2>/dev/null); then
            
            if [ "$input_normalized" = "$output_normalized" ]; then
                echo "Line $line_num: ✓ JSON semantically equal"
            else
                echo "Line $line_num: ✗ JSON differs"
                echo "  Expected: $input_normalized"
                echo "  Actual:   $output_normalized"
                all_passed=false
            fi
        else
            echo "Line $line_num: ✗ JSON parsing failed"
            echo "  Input:  $input_json"
            echo "  Output: $output_line"
            all_passed=false
        fi
        
        ((line_num++))
    done < "$input_file" 3< "$output_file"
    
    if [ "$all_passed" = true ]; then
        echo "✓ All JSON validations passed with jq!"
        return 0
    else
        echo "✗ JSON validation failed with jq!"
        return 1
    fi
}

basic_field_check() {
    local input_file="$1"
    local output_file="$2"
    local cookie="$3"
    
    echo "Performing basic field presence check..."
    
    # Extract some key fields that should be present
    local expected_fields=("name" "version" "type" "src" "dst" "payload")
    local all_found=true
    
    for field in "${expected_fields[@]}"; do
        if grep -q "\"$field\":" "$input_file" && grep -q "\"$field\":" "$output_file"; then
            echo "✓ Field '$field' found in both input and output"
        else
            echo "✗ Field '$field' missing or mismatched"
            all_found=false
        fi
    done
    
    if [ "$all_found" = true ]; then
        echo "✓ Basic field validation passed!"
        return 0
    else
        echo "✗ Basic field validation failed!"
        return 1
    fi
}

main() {
    local input_file="$1"
    local output_file="$2"
    local cookie="${3:-<01> @cee:}"
    
    if [ $# -lt 2 ]; then
        echo "Usage: $0 <input_file> <output_file> [cookie]"
        echo "Example: $0 testsuites/qradar_json rsyslog.out.log '<01> @cee:'"
        exit 1
    fi
    
    echo "Validating JSON equality between $input_file and $output_file"
    echo "Using cookie: $cookie"
    echo "--------------------------------------------------"
    
    # Check if jq is available and decide validation method
    if command -v jq >/dev/null 2>&1; then
        # jq is available, use it for validation (exit immediately if it fails)
        if validate_json_with_jq "$input_file" "$output_file" "$cookie"; then
            exit 0
        else
            echo "✗ JSON validation failed with jq!"
            exit 1
        fi
    else
        # jq not available, use basic validation
        echo "jq not available, using basic field validation..."
        if basic_field_check "$input_file" "$output_file" "$cookie"; then
            echo "Note: Used basic validation due to jq unavailability"
            exit 0
        else
            echo "✗ JSON validation failed!"
            exit 1
        fi
    fi
}

main "$@"