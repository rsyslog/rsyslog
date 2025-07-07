anon_cc_nbrs.py
===============

This plugin permits to anonymize credit card numbers.
Note: interface.input **must** be set to "msg".

How to use?
-----------

You need to load mmexternal plugin as interface and configure it
to call anon_cc_nbrs.py.

Snippet of rsyslog.conf:

module(load="mmexternal") # do this only once

if(*whatever filter here*) then {
    action(type="mmexternal" binary="/path/to/anon_cc_nbrs.py"
           interface.input="msg")
   
}
