$ActionExecOnlyWhenPreviousIsSuspended
--------------------------------------

**Type:** action configuration parameter

**Default:** off

**Description:**

This parameter allows to specify if actions should always be executed
("off," the default) or only if the previous action is suspended ("on").
This parameter works hand-in-hand with the multiple actions per selector
feature. It can be used, for example, to create rules that automatically
switch destination servers or databases to a (set of) backup(s), if the
primary server fails. Note that this feature depends on proper
implementation of the suspend feature in the output module. All built-in
output modules properly support it (most importantly the database write
and the syslog message forwarder).

This selector processes all messages it receives (\*.\*). It tries to
forward every message to primary-syslog.example.com (via tcp). If it can
not reach that server, it tries secondary-1-syslog.example.com, if that
fails too, it tries secondary-2-syslog.example.com. If neither of these
servers can be connected, the data is stored in /var/log/localbuffer.
Please note that the secondaries and the local log buffer are only used
if the one before them does not work. So ideally, /var/log/localbuffer
will never receive a message. If one of the servers resumes operation,
it automatically takes over processing again.

We strongly advise not to use repeated line reduction together with
ActionExecOnlyWhenPreviousIsSuspended. It may lead to "interesting" and
undesired results (but you can try it if you like).

Example::

    *.* @@primary-syslog.example.com
    $ActionExecOnlyWhenPreviousIsSuspended on
    & @@secondary-1-syslog.example.com    # & is used to have more than one action for
    & @@secondary-2-syslog.example.com    # the same selector - the multi-action feature
    & /var/log/localbuffer
    $ActionExecOnlyWhenPreviousIsSuspended off # to re-set it for the next selector
