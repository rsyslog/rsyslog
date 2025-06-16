docker specifics
----------------

This page talks about docker, as this is the platform the rsyslog
team has hands-on experience with. Some or all of the information
might also apply to other solutions.

Potential Trouble causes:

- container terminate timeout

  By default, a docker container has 10 seconds to shut down. If rsyslog
  is running with a large queue that needs to be persisted to disk,
  that amount of time might be insufficient. This can lead to a hard
  kill of rsyslog and potentially cause queue corruption.

- shared work directories

  Shared work directories call for problems and shall be avoided. If
  multiple instances use the same work directory, the may even overwrite
  some files, resulting in a total mess. Each container instance should
  have its own work directory.

