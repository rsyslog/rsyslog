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

Use Rainerscript rotation parameters for fixed-length syslog files
------------------------------------------------------------------

Lets assume you do not want to spend more than 100 MB hard disc space
for you logs. With rsyslog you can configure the
``rotation.sizeLimit`` and ``rotation.sizeLimitCommand`` parameters of
the :doc:`omfile action <../configuration/modules/omfile>` to achieve
this. The Rainerscript fragment below writes all messages to a file and
rotates it once the size limit is exceeded.

.. code-block:: rsyslog

    action(
        type="omfile"
        file="/var/log/log_rotation.log"
        rotation.sizeLimit="52428800"        # 50 MiB per file
        rotation.sizeLimitCommand="/home/me/log_rotation_script"
        template="RSYSLOG_TraditionalFileFormat"
    )

When the configured limit is reached, rsyslog executes the command
specified in ``rotation.sizeLimitCommand``. In our case it runs
``/home/me/log_rotation_script`` which contains a single command:

::

    mv -f /var/log/log_rotation.log /var/log/log_rotation.log.1

This moves the original log to a kind of backup log file. After the
action was successfully performed rsyslog creates a new
``/var/log/log_rotation.log`` file and fills it with new logs. So the
latest logs are always in ``log_rotation.log``. The two rotation
parameters are documented in
:doc:`../reference/parameters/omfile-rotation-sizelimit` and
:doc:`../reference/parameters/omfile-rotation-sizelimitcommand`.

.. note::

   Older examples use the ``$outchannel`` directive. That syntax maps to
   the same rotation parameters shown above but is kept solely for
   backward compatibility. New configurations should always use
   Rainerscript objects and attributes.

Conclusion
----------

With this approach two files for logging are used, each with a maximum
size of 50 MB. So we can say we have successfully configured a log
rotation which satisfies our requirement. We keep the logs at a
fixed-size level of 100 MB.

