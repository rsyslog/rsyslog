#!/bin/bash
# This test verifies that array comparisons (both == and !=) work correctly
# when the left-hand operand is a JSON-type property or local variable (e.g. $.testvar).
# This acts as the regression test for issue #487.
#
# Intent: Ensure array comparison operators correctly inspect all elements of the
# right-hand array when compared against a local variable (PROP_LOCAL_VAR), which 
# evaluates to the JSON-type 'J'.
#
# Oracle:
# - Test 1 (== match): Should pass because $.testvar ("match_me") is in the list.
# - Test 2 (== no match): Should fail because $.testvar is not in the list.
# - Test 3 (!= match): Should pass because $.testvar is not in the list (so all are different).
# - Test 4 (!= no match): Should fail because $.testvar is in the list (so not all are different).
#
# If the bug is present, Test 4 incorrectly passes because it only compares $.testvar to "other1".
# Therefore, success is defined by "T1_PASS" and "T3_PASS" being present, and "T2_FAIL" and "T4_FAIL" being absent.

. ${srcdir:=.}/diag.sh init snd-array-cmp-json.sh
generate_conf
add_conf '
template(name="out" type="string" string="%msg% : %$.status%\n")

set $.testvar = "match_me";
set $.testnum = 42;

# Test 1: == match (should fire)
if $.testvar == ["other1", "match_me", "other2"] then {
    set $.status = "T1_PASS";
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="out")
}

# Test 2: == no match (should NOT fire)
if $.testvar == ["other1", "other2"] then {
    set $.status = "T2_FAIL";
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="out")
}

# Test 3: != match (should fire since match_me is not in the list)
if $.testvar != ["other1", "other2"] then {
    set $.status = "T3_PASS";
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="out")
}

# Test 4: != no match (should NOT fire since match_me is in the list)
if $.testvar != ["other1", "match_me", "other2"] then {
    set $.status = "T4_FAIL";
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="out")
}

# Test 5: == match numeric on JSON variable (should fire)
if $.testnum == 42 then {
    set $.status = "T5_PASS";
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="out")
}

# Test 6: != no match numeric on JSON variable (should NOT fire)
if $.testnum != 42 then {
    set $.status = "T6_FAIL";
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="out")
}

# Test 7: == no match numeric on JSON variable (should NOT fire)
if $.testnum == 43 then {
    set $.status = "T7_FAIL";
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="out")
}

# Test 8: != match numeric on JSON variable (should fire)
if $.testnum != 43 then {
    set $.status = "T8_PASS";
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="out")
}
'

startup
injectmsg  # Injects a single default log message
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown 

content_check "T1_PASS"
content_check "T3_PASS"
content_check "T5_PASS"
content_check "T8_PASS"
assert_content_missing "T2_FAIL"
assert_content_missing "T4_FAIL"
assert_content_missing "T6_FAIL"
assert_content_missing "T7_FAIL"

exit_test
