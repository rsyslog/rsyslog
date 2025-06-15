How to create a debug log
=========================

Creating a debug log is actually quite simple. You just have to add a 
few lines to the configuration file.

Regular debug output
--------------------

Add the following right at the beginning of the rsyslog.conf file. This
will ensure that debug support is the first thing to enable when the 
rsyslog service is started:

::

	$DebugFile /var/log/rsyslog.debug
	$DebugLevel 2

The actual file path and name may be changed if required.

Having set the above, when rsyslog is restarted it will produce a continuous
debug file. 

Debug on Demand
---------------

For having rsyslog be ready to create a debug log (aka Debug on Demand), the
settings are a little different. 

::

	$DebugFile /var/log/rsyslog.debug
	$DebugLevel 1

Now, rsyslog will not create a debug log on restart, but wait for a USR signal
to the pid. When sent, the debug output will be triggered. When sent again, 
debug output will be stopped.

::

    kill -USR1 `cat /var/run/rsyslogd.pid`

Notes
-----

- Having debug output enabled, the debug file will grow very quickly. Make sure
  to not have it enabled permanently. The file will eventually fill up the disk.
- Debug mode is not to be used in a productive environment as a permanent setting.
  It will affect the processing and performance.

See Also
--------

- :doc:`Troubleshooting <../troubleshooting/troubleshoot>` doc page.
- :doc:`Debug Support<../troubleshooting/debug>` doc page with more detailed 
  information.

