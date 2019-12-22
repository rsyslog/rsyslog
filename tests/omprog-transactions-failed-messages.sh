#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# This test tests omprog with the confirmMessages=on and useTransactions=on
# parameters, with the external program returning an error on certain
# messages.

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
    action(
        type="omprog"
        binary=`echo $srcdir/testsuites/omprog-transactions-bin.sh --failed_messages`
        template="outfmt"
        name="omprog_action"
        queue.type="Direct"  # the default; facilitates sync with the child process
        queue.dequeueBatchSize="6"
        confirmMessages="on"
        useTransactions="on"
        action.resumeRetryCount="10"
        action.resumeInterval="1"
    )
}
'
startup
injectmsg 0 10
shutdown_when_empty
wait_shutdown

# Since the transaction boundaries are not deterministic, we cannot check for
# an exact expected output. We must check the output programmatically.

transaction_state="NONE"
status_expected=true
messages_to_commit=()
messages_processed=()
line_num=1
error=

while IFS= read -r line; do
    if [[ $status_expected == true ]]; then
        case "$transaction_state" in
        "NONE")
            if [[ "$line" != "<= OK" ]]; then
                error="expecting an OK status from script"
                break
            fi
            ;;
        "STARTED")
            if [[ "$line" != "<= OK" ]]; then
                error="expecting an OK status from script"
                break
            fi
            transaction_state="ACTIVE"
            ;;
        "ACTIVE")
            if [[ "$line" == "<= Error: could not process log message" ]]; then
                #
                # TODO: Issue #2420: Deferred messages within a transaction are
                # not retried by rsyslog.
                # If that's the expected behavior, what's then the difference
                # between the RS_RET_OK and the RS_RET_DEFER_COMMIT return codes?
                # If that's not the expected behavior, the following lines must
                # be removed when the bug is solved.
                #
                # (START OF CODE THAT WILL POSSIBLY NEED TO BE REMOVED)
                messages_processed+=("${messages_to_commit[@]}")
                unset "messages_processed[${#messages_processed[@]}-1]"
                # (END OF CODE THAT WILL POSSIBLY NEED TO BE REMOVED)

                messages_to_commit=()
                transaction_state="NONE"
            elif [[ "$line" != "<= DEFER_COMMIT" ]]; then
                error="expecting a DEFER_COMMIT status from script"
                break
            fi
            ;;
        "COMMITTED")
            if [[ "$line" != "<= OK" ]]; then
                error="expecting an OK status from script"
                break
            fi
            messages_processed+=("${messages_to_commit[@]}")
            messages_to_commit=()
            transaction_state="NONE"
            ;;
        esac
        status_expected=false;
    else
        if [[ "$line" == "=> BEGIN TRANSACTION" ]]; then
            if [[ "$transaction_state" != "NONE" ]]; then
                error="unexpected transaction start"
                break
            fi
            transaction_state="STARTED"
        elif [[ "$line" == "=> COMMIT TRANSACTION" ]]; then
            if [[ "$transaction_state" != "ACTIVE" ]]; then
                error="unexpected transaction commit"
                break
            fi
            transaction_state="COMMITTED"
        else
            if [[ "$transaction_state" != "ACTIVE" ]]; then
                error="unexpected message outside a transaction"
                break
            fi
            if [[ "$line" != "=> msgnum:"* ]]; then
                error="unexpected message contents"
                break
            fi
            prefix_to_remove="=> "
            messages_to_commit+=("${line#$prefix_to_remove}")
        fi
        status_expected=true;
    fi
    ((line_num++))
done < $RSYSLOG_OUT_LOG

if [[ -z "$error" && "$transaction_state" != "NONE" ]]; then
    error="unexpected end of file (transaction state: $transaction_state)"
fi

if [[ -n "$error" ]]; then
    echo "$RSYSLOG_OUT_LOG: line $line_num: $error"
    cat $RSYSLOG_OUT_LOG
    error_exit 1
fi

# Since the order in which failed messages are retried by rsyslog is not
# deterministic, we sort the processed messages before checking them.
IFS=$'\n' messages_sorted=($(sort <<<"${messages_processed[*]}"))
unset IFS

expected_messages=(
    "msgnum:00000000:"
    "msgnum:00000001:"
    "msgnum:00000002:"
    "msgnum:00000003:"
    "msgnum:00000004:"
    "msgnum:00000005:"
    "msgnum:00000006:"
    "msgnum:00000007:"
    "msgnum:00000008:"
    "msgnum:00000009:"
)
if [[ "${messages_sorted[*]}" != "${expected_messages[*]}" ]]; then
    echo "unexpected set of processed messages:"
    printf '%s\n' "${messages_processed[@]}"
    echo "contents of $RSYSLOG_OUT_LOG:"
    cat $RSYSLOG_OUT_LOG
    error_exit 1
fi

exit_test
