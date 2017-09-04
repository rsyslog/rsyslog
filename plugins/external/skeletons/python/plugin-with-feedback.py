#!/usr/bin/env python3
"""A skeleton for a Python rsyslog output plugin with error handling
and transaction support. Requires Python 3.

To integrate a plugin based on this skeleton with rsyslog, configure an
'omprog' action like the following:
    action(type="omprog"
        binary="/usr/bin/myplugin.py"
        confirmMessages="on"
        useTransactions="on"  # or "off" if you don't need transactions
        ...)

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

import sys
import os
import logging


# Global definitions specific to your plugin
outfile = None


class RecoverableError(Exception):
    """An error that has caused the processing of the current message or
    transaction to fail, but does not require restarting the plugin.

    An example of such an error would be a temporary loss of connection to
    a database or a server. If such an error occurs in the onBeginTransation,
    onMessage or onCommitTransaction functions, your plugin should wrap it
    in a RecoverableError before raising it. For example:

        try:
            # code that connects to a database
        except DbConnectionError as e:
            raise RecoverableError from e

    Recoverable errors will cause the 'omprog' action to be temporarily
    suspended by rsyslog, during a period that can be configured using the
    "action.resumeInterval" action parameter. When the action is resumed,
    rsyslog will resend the failed message or transaction to your plugin.
    """


def onInit():
    """Do everything that is needed to initialize processing (e.g. open files,
    create handles, connect to systems...).
    """
    # Apart from processing the logs received from rsyslog, you want your plugin
    # to be able to report its own logs in some way. This will facilitate
    # diagnosing problems and debugging your code. Here we set up the standard
    # Python logging system to output the logs to stderr.
    logging.basicConfig(stream=sys.stderr,
                        level=logging.WARNING,
                        format='%(asctime)s %(levelname)s %(message)s')

    # In the rsyslog configuration, you can configure the 'omprog' action to
    # capture the stderr of your plugin by specifying the action's "output"
    # parameter. However, note that this parameter is intended for debugging
    # purposes and cannot be used on a permanent basis (in particular, it will
    # not work well if rsyslog decides to launch multiple instances of your
    # plugin, since this would cause concurrent writes to the output file).
    # As an alternative to logging to stderr, you can log to a per-process
    # file, as shown here:
    # logging.basicConfig(filename="myplugin-{}.log".format(os.getpid()), ...)

    # This is an example of a debug log. (Note that for debug logs to be
    # emitted you must set 'level' to logging.DEBUG above.)
    logging.debug("onInit called")

    # For illustrative purposes, this plugin skeleton appends the received logs
    # to a file. When implementing your plugin, remove the following code.
    global outfile
	outfile = open("/tmp/logfile", "w")


def onBeginTransaction():
    """Begin the processing of a batch of messages.

    This function is invoked only when the "useTransactions" parameter is
    configured to "on" in the 'omprog' action.

    You can implement this function to e.g. start a database transaction.

    Raises:
        RecoverableError: If a recoverable error occurs. The message or the
            transaction will be retried without restarting the plugin.
        Exception: If a non-recoverable error occurs. The plugin will be
            restarted before retrying the message or the transaction.
    """
    logging.debug("onBeginTransaction called")


def onMessage(msg):
    """Process one log message received from rsyslog (e.g. send it to a
    database).

    If this function raises an error and the "useTransactions" parameter is
    configured to "on" in the 'omprog' action, rsyslog will retry the full
    batch of messages. Otherwise, if "useTransactions" is set to "off", only
    this message will be retried.

    Args:
        msg (str): the log message. Does NOT include a trailing newline.

    Raises:
        RecoverableError: If a recoverable error occurs. The message or the
            transaction will be retried without restarting the plugin.
        Exception: If a non-recoverable error occurs. The plugin will be
            restarted before retrying the message or the transaction.
    """
    logging.debug("onMessage called")

    # It is recommended to check that the "useTransactions" flag is
    # appropriately configured in 'omprog'. If your plugin requires
    # transactions, you can check that they are enabled as follows:
    # global inTransaction
    # assert inTransaction, "This plugin requires transactions to be enabled"

    # Otherwise, if your plugin does not support transactions, you can check
    # that they are disabled as follows:
    # global inTransaction
    # assert not inTransaction, "This plugin does not support transactions"

    # For illustrative purposes, this plugin skeleton appends the received logs
    # to a file. When implementing your plugin, remove the following code.
    global outfile
    outfile.write(msg)
    outfile.write("\n")
    outfile.flush()


def onCommitTransaction():
    """Complete the processing of a batch of messages.

    This function is invoked only when the "useTransactions" parameter is
    configured to "on" in the 'omprog' action.

    You can implement this function to e.g. commit a database transaction.

    Raises:
        RecoverableError: If a recoverable error occurs. The transaction
            will be retried without restarting the plugin.
        Exception: If a non-recoverable error occurs. The plugin will be
            restarted before retrying the transaction.
    """
    logging.debug("onCommitTransaction called")


def onRollbackTransaction():
    """Cancel the processing of a batch of messages.

    This function is invoked only when the "useTransactions" parameter is
    configured to "on" in the 'omprog' action, and when the "onMessage"
    function has raised a (recoverable or non-recoverable) error for one of
    the messages in the batch. It is also invoked if "onCommitTransaction"
    raises an error.

    You can implement this function to e.g. rollback a database transaction.

    This function should not raise any error. If it does, the error will be
    logged as a warning and ignored.
    """
    logging.debug("onRollbackTransaction called")


def onExit():
    """Do everything that is needed to finish processing (e.g. close files,
    handles, disconnect from systems...). This is being called immediately
    before exiting.

    This function should not raise any error. If it does, the error will be
    logged as a warning and ignored.
    """
    logging.debug("onExit called")

    # For illustrative purposes, this plugin skeleton appends the received logs
    # to a file. When implementing your plugin, remove the following code.
    global outfile
    outfile.close()


"""
-------------------------------------------------------
This is plumbing that DOES NOT need to be CHANGED
-------------------------------------------------------
This is the main loop that receives messages from rsyslog via stdin,
invokes the above entrypoints, and provides status codes to rsyslog
via stdout. In most cases, modifying this code should not be necessary.

You will have to change the code below if you need the following
advanced features:
    * Custom begin/end transaction marks: if you have configured the
    'omprog' action in rsyslog to use your own marks for transaction
    boundaries (instead of the default "BEGIN TRANSACTION" and "COMMIT
    TRANSACTION" messages), modify the code below accordingly.
    * Partial transaction commits: this skeleton confirms all messages
    inside a transaction using the "DEFER_COMMIT" status code. If you
    want to return the "PREVIOUS_COMMITED" or "OK" status codes within
    transactions, you will need to modify the code below. See
    http://www.rsyslog.com/doc/v8-stable/development/dev_oplugins.html
    for information about these status codes. Note that rsyslog will not
    send the "COMMIT TRANSACTION" mark if the last message in the
    transaction is confirmed with an "OK" status code.
"""
try:
    onInit()
except Exception as e:
    # If an error occurs during initialization, log it and terminate. The
    # 'omprog' action will eventually restart the program.
    logging.exception("Initialization error, exiting program")
    sys.exit(1)

# Tell rsyslog we are ready to start processing messages:
print("OK", flush=True)

inTransaction = False
endedWithError = False
try:
    line = sys.stdin.readline()
    while line:
        line = line.rstrip('\n')
        try:
            try:
                if line == "BEGIN TRANSACTION":
                    onBeginTransaction()
                    inTransaction = True
                    status = "OK"
                elif line == "COMMIT TRANSACTION":
                    onCommitTransaction()
                    inTransaction = False
                    status = "OK"
                else:
                    onMessage(line)
                    status = "DEFER_COMMIT" if inTransaction else "OK"

            except Exception:
                # If a transaction was in progress, call onRollbackTransaction
                # to facilitate cleaning up the transaction state. Note that
                # rsyslog does not support this notification. (It probably
                # should, to allow the plugin to be notified in case the
                # transaction fails in the rsyslog side.)
                if inTransaction:
                    try:
                        onRollbackTransaction()
                    except Exception as ignored:
                        ignored.__suppress_context__ = True
                        logging.warning("Exception ignored in onRollbackTransaction", exc_info=True)
                    inTransaction = False
                raise

        except RecoverableError as e:
            # Any line written to stdout that is not a status code will be
            # treated as a recoverable error by 'omprog', and cause the action
            # to be temporarily suspended. In this skeleton, we simply return
            # a one-line representation of the Python exception. (If debugging
            # is enabled in rsyslog, this line will appear in the debug logs.)
            status = repr(e)
            # We also log the complete exception to stderr (or to the logging
            # handler(s) configured in doInit, if any).
            logging.exception(e)
            inTransaction = False

        # Send the status code (or the one-line error message) to rsyslog:
        print(status, flush=True)
        line = sys.stdin.readline()

except Exception:
    # If a non-recoverable error occurs, log it and terminate. The 'omprog'
    # action will eventually restart the program.
    logging.exception("Unrecoverable error, exiting program")
    endedWithError = True

try:
    onExit()
except Exception:
    logging.warning("Exception ignored in onExit", exc_info=True)

if endedWithError:
    sys.exit(1)
else:
    sys.exit(0)
