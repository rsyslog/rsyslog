fac-sever-rewrite.py
====================

This plugin permits rewrite the facility and severity of messages.
The code needs to be customized to match the actual use case.

The code as is simply increments the message severity by one.

Note: interface.input **must** must be set to "fulljson".

How to use?
-----------

You need to load mmexternal plugin as interface and configure it
to call fac-sever-rewrite.py:

Snippet of rsyslog.conf:

module(load="mmexternal") # do this only once

if( *whatever filter here* ) then {
    action(type="mmexternal" binary="/path/to/fac-sever-rewrite.py"
           interface.input="fulljson")
   
}
