## rsyslog base container for Alpine

This container just provides a fresh rsyslog with frequently used
modules on top of Alpine.

Use this, if you

a) want to build your own container based on current rsyslog
b) want to run rsyslog with a custom config by you

note that in case b) you need to make sure that you use volumes
etc properly. No specific support for this has been added.
