## rsyslog base container for CentOS 7

This container just provides a fresh rsyslog with frequently used
modules on top of CentOS 7.

Use this, if you

a) want to build your own container based on current rsyslog
b) want to run rsyslog with a custom config by you
c) want to run a client machine where rsyslog processes log messages
   (the default CentOS 7 config does NOT work inside a container, but
   this container has a corrected config!)

note that in case b) you need to make sure that you use volumes
etc properly. No specific support for this has been added.
