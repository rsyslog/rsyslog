Debugging
=========

**Author:** Pascal Withopf <pascalwithopf1@gmail.com>

Target audience are developers and users who need to debug an error with tests.
For debugging with rsyslog.conf see :doc:`troubleshooting <../../05_how-to_guides/troubleshoot>`.

Debugging with tests
--------------------

| When you want to solve a specific problem you will probably create a test
| and want to debug with it instead of configuring rsyslog. If you want to
| write a debug log you need to open the file **../rsyslog/tests/diag.sh**
| and delete the **#** in front of the two lines:

| **export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"**
| **export RSYSLOG_DEBUGLOG="log"**

| A debug log will be written now, but remember to put the **#** back again
| before committing your changes. Otherwise it won't work.

Memory debugging
----------------

| You can't use multiple memory debugger at the same time. This will resort
| in errors. Also remember to undo all changes in diag.sh after you are done,
| because it will also resort in errors if you commit them with your work.

Valgrind
~~~~~~~~

| If you want to use Valgrind you need to enable it for tests.
| To do that open the file **../rsyslog/tests/diag.sh** and delete the **#**
| in front of the line:
| **valgrind="valgrind --malloc-fill=ff --free-fill=fe --log-fd=1"**
| This will enable valgrind and you will have extra debugging in your test-suite.log file.

Address sanitizer
~~~~~~~~~~~~~~~~~

| If you want to use address sanitizer you need to set your CFLAGS. Use this command:
| **export CFLAGS="-g -fsanitizer=address"**
| After this is done you need to configure and build rsyslog again, otherwise it won't work.

