#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify case-sensitive, whole-string POSIX fnmatch semantics for scalar and
# array patterns, including wildcard, bracket, escape, empty, and negative cases.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string"
         string="dynamic=%$.dynamic% exact=%$.exact% mismatch=%$.mismatch% negated=%$.negated% empty=%$.empty% star=%$.star% question=%$.question% question_miss=%$.question_miss% bracket=%$.bracket% escape=%$.escape% array=%$.array% array_miss=%$.array_miss% case=%$.case% pathslash=%$.pathslash%\n")

set $.subject = "alpha.beta";
set $.pattern = "alpha?beta";
set $.dynamic = 0;
set $.exact = 0;
set $.mismatch = 0;
set $.negated = 0;
set $.empty = 0;
set $.star = 0;
set $.question = 0;
set $.question_miss = 0;
set $.bracket = 0;
set $.escape = 0;
set $.array = 0;
set $.array_miss = 0;
set $.case = 0;
set $.pathslash = 0;

if $.subject fnmatch $.pattern then { set $.dynamic = 1; }
if "abc" fnmatch "abc" then { set $.exact = 1; }
if "abc" fnmatch "abd" then { set $.mismatch = 1; }
if not ("abc" fnmatch "abd") then { set $.negated = 1; }
if "" fnmatch "" then { set $.empty = 1; }
if "" fnmatch "*" then { set $.star = 1; }
if "b" fnmatch "?" then { set $.question = 1; }
if "bb" fnmatch "?" then { set $.question_miss = 1; }
if "b" fnmatch "[a-c]" then { set $.bracket = 1; }
if "a*b" fnmatch "a\\*b" then { set $.escape = 1; }
if $.subject fnmatch ["unmatched", "alpha*"] then { set $.array = 1; }
if $.subject fnmatch ["unmatched", "no-match*"] then { set $.array_miss = 1; }
if "abc" fnmatch "A*" then { set $.case = 1; }
if ".hidden/path" fnmatch "*" then { set $.pathslash = 1; }

if $programname == "testtag" then {
    action(type="omfile" template="outfmt" file="'${RSYSLOG_OUT_LOG}'")
}
'
startup
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host testtag - - - fnmatch-fixture'
shutdown_when_empty
wait_shutdown
export EXPECTED="dynamic=1 exact=1 mismatch=0 negated=1 empty=1 star=1 question=1 question_miss=0 bracket=1 escape=1 array=1 array_miss=0 case=0 pathslash=1"
cmp_exact
exit_test
