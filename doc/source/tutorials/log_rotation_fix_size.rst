Log rotation with rsyslog
=========================

*Written by Michael Meckelein*

Situation
---------

Your environment does not allow you to store tons of logs? You have
limited disc space available for logging, for example you want to log to
a 124 MB RAM usb stick? Or you do not want to keep all the logs for
months, logs from the last days is sufficient? Think about log rotation.

Log rotation based on a fixed log size
--------------------------------------

This small but hopefully useful article will show you the way to keep
your logs at a given size. The following sample is based on rsyslog
illustrating a simple but effective log rotation with a maximum size
condition.

Use Output Channels for fixed-length syslog files
-------------------------------------------------

Lets assume you do not want to spend more than 100 MB hard disc space
for you logs. With rsyslog you can configure Output Channels to achieve
this. Putting the following directive

::

    # start log rotation via outchannel
    # outchannel definition
    $outchannel log_rotation,/var/log/log_rotation.log, 52428800,/home/me/./log_rotation_script 
    #  activate the channel and log everything to it 
    *.* :omfile:$log_rotation
    # end log rotation via outchannel

to rsyslog.conf instruct rsyslog to log everything to the destination
file '/var/log/log\_rotation.log' until the give file size of 50 MB is
reached. If the max file size is reached it will perform an action. In
our case it executes the script /home/me/log\_rotation\_script which
contains a single command:

::

    mv -f /var/log/log_rotation.log /var/log/log_rotation.log.1

This moves the original log to a kind of backup log file. After the
action was successfully performed rsyslog creates a new
/var/log/log\_rotation.log file and fill it up with new logs. So the
latest logs are always in log\_rotation.log.

Conclusion
----------

With this approach two files for logging are used, each with a maximum
size of 50 MB. So we can say we have successfully configured a log
rotation which satisfies our requirement. We keep the logs at a
fixed-size level of 100 MB.

