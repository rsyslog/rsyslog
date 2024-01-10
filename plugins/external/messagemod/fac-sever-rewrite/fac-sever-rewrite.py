#!/usr/bin/env python3

"""
A message modification plugin to rewrite message facility and severity.

Example usage:

    module(load="mmexternal")
    action(type="mmexternal"
           binary="fac-sever-rewrite.py -s notice"
           interface.input="fulljson")

Note: this script should be customized according to your needs.

Copyright (c) 2014-2023 by Adiscon GmbH and James Howe

This file is part of rsyslog.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0
     -or-
     see COPYING.ASL20 in the source distribution

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import argparse
import json
import sys
import syslog

args = argparse.Namespace()


def _facility(value) -> int:
    try:
        return int(value)
    except ValueError:
        name = 'LOG_' + value.upper()
        const = getattr(syslog, name)
        # Python's constants are for direct masking,
        #   shift back to raw syslog value
        return const >> 3


def _severity(value) -> int:
    try:
        return int(value)
    except ValueError:
        name = 'LOG_' + value.upper()
        const = getattr(syslog, name)
        return const


def on_init():
    """
    Do everything that is needed to initialize processing
    (e.g. open files, create handles, connect to systems...)
    """
    parser = argparse.ArgumentParser(prog='fac-sever-rewrite.py')
    parser.add_argument('-f', '--facility', required=False, type=_facility,
                        help='Alias or integer of new facility to set')
    parser.add_argument('-s', '--severity', required=False, type=_severity,
                        help='Alias or integer of new severity to set')
    parser.parse_args(namespace=args)


def on_receive(msg: dict) -> dict:
    """
    This is the entry point where actual work needs to be done. It receives
    the message from rsyslog and now needs to examine it, do any processing
    necessary. The to-be-modified properties (one or many) need to be pushed
    back to stdout, in JSON format, with no interim line breaks and a line
    break at the end of the JSON. If no field is to be modified, empty
    json ("{}") needs to be emitted.
    Note that no batching takes place (contrary to the output module skeleton)
    and so each message needs to be fully processed (rsyslog will wait for the
    reply before the next message is pushed to this module).
    """
    changes = {}
    if args.facility is not None:
        changes['syslogfacility'] = args.facility
    if args.severity is not None:
        changes['syslogseverity'] = args.severity
    return changes


def on_exit():
    """
    Do everything that is needed to finish processing
    (e.g. close files, handles, disconnect from systems...).
    This is being called immediately before exiting.
    """
    pass


def main():
    """
    -------------------------------------------------------
    This is plumbing that DOES NOT need to be CHANGED
    -------------------------------------------------------
    """
    on_init()
    for line in sys.stdin:
        msg = json.loads(line)
        changes = on_receive(msg)
        print(json.dumps(changes), flush=True)
    on_exit()


if __name__ == '__main__':
    main()

