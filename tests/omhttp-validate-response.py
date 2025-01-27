import json
import argparse
import sys
from collections import defaultdict


def parse_json_lines_to_memory(file, filename):
    """Read a file and reconstruct complete JSON objects into a list."""
    buffer = ""
    line_number = 0
    parsed_objects = []
    for line in file:
        line_number += 1
        buffer += line.strip()
        try:
            # Attempt to parse the JSON object
            parsed_objects.append(json.loads(buffer))
            buffer = ""  # Clear the buffer after successful parsing
        except json.JSONDecodeError:
            # If parsing fails, keep buffering lines
            continue
    if buffer:
        # If there's leftover data in the buffer, raise an error
        raise ValueError(
            f"Incomplete JSON object found at the end of the file '{filename}'. "
            f"Last buffered content: {buffer}"
        )
    return parsed_objects


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Validate and process log files for errors and responses.')
    parser.add_argument('--error', action='store', type=str, required=True, help='Path to the error file.')
    parser.add_argument('--response', action='store', type=str, required=True, help='Path to the response file.')
    parser.add_argument('--max-errors', action='store', type=int, default=10, help='Maximum number of errors to display.')
    args = parser.parse_args()

    messages = defaultdict(dict)
    errors = []  # Collect errors for reporting at the end

    # Load the error file into memory
    try:
        with open(args.error, "r") as error_f:
            error_data = parse_json_lines_to_memory(error_f, args.error)
            for json_obj in error_data:
                try:
                    records = json.loads(json_obj['request']['postdata'])
                    if records:
                        for i, val in enumerate(records['records']):
                            msgnum = val['value']['msgnum']
                            messages[msgnum]['response'] = json_obj['response']
                            messages[msgnum]['index'] = i
                except KeyError as e:
                    errors.append(
                        f"Missing key {e} in error file '{args.error}'. "
                        f"Problematic JSON object: {json_obj}"
                    )
                except json.JSONDecodeError as e:
                    errors.append(
                        f"Error decoding 'postdata' in error file '{args.error}'. "
                        f"Content: {json_obj['request']['postdata']}. Error: {e}"
                    )
    except FileNotFoundError as e:
        errors.append(f"File not found: {args.error}. Please check the file path.")
    except ValueError as e:
        errors.append(str(e))

    # Load the response file into memory
    try:
        with open(args.response, "r") as response_f:
            response_data = parse_json_lines_to_memory(response_f, args.response)
            for json_obj in response_data:
                try:
                    msgnum = json_obj['message']['msgnum']
                    code = json_obj['response']['code']
                    body = json_obj['response']['body']
                    batch_index = json_obj['response']['batch_index']

                    if msgnum not in messages:
                        errors.append(
                            f"Message {msgnum} in response file '{args.response}' "
                            f"does not exist in the error file '{args.error}'."
                        )
                        continue

                    if messages[msgnum]['response']['status'] != code:
                        errors.append(
                            f"Status code mismatch for message {msgnum}:\n"
                            f"  Expected: {messages[msgnum]['response']['status']}\n"
                            f"  Found: {code}\n"
                            f"  Context: {json_obj}"
                        )

                    if messages[msgnum]['response']['message'] != body:
                        errors.append(
                            f"Message body mismatch for message {msgnum}:\n"
                            f"  Expected: {messages[msgnum]['response']['message']}\n"
                            f"  Found: {body}\n"
                            f"  Context: {json_obj}"
                        )

                    if messages[msgnum]['index'] != batch_index:
                        errors.append(
                            f"Batch index mismatch for message {msgnum}:\n"
                            f"  Expected: {messages[msgnum]['index']}\n"
                            f"  Found: {batch_index}\n"
                            f"  Context: {json_obj}"
                        )
                except KeyError as e:
                    errors.append(
                        f"Missing key {e} in response file '{args.response}'. "
                        f"Problematic JSON object: {json_obj}"
                    )
    except FileNotFoundError as e:
        errors.append(f"File not found: {args.response}. Please check the file path.")
    except ValueError as e:
        errors.append(str(e))

    # Report errors, limited by --max-errors
    if errors:
        print(f"Validation completed with {len(errors)} errors. Showing the first {min(len(errors), args.max_errors)} errors:\n")
        for error in errors[:args.max_errors]:
            print(f"- {error}")
        # Exit with non-zero code to indicate errors
        sys.exit(1)
    else:
        print("Validation completed successfully. No errors found.")
        # Exit with zero code to indicate success
        sys.exit(0)
