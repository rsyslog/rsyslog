split()
=======

Purpose
-------

Splits a string into a JSON array using a specified separator.

Syntax
------

.. code-block:: none

   split(string, separator)

Parameters
----------

string
   The input string to be split.

separator
   The delimiter string used to split the input. Can be a single character
   or a multi-character string.

Return Value
------------

Returns a JSON array containing the split substrings.

Examples
--------

.. code-block:: none

   # Single-character separator
   set $!tags = "error,warning,info";
   set $!tag_array = split($!tags, ",");
   # Result: ["error", "warning", "info"]

   # Multi-character separator
   set $!data = "item1::item2::item3";
   set $!items = split($!data, "::");
   # Result: ["item1", "item2", "item3"]

Complete Example
----------------

This example demonstrates splitting a comma-separated list of recipients
and forwarding each one to a different destination based on their department.

.. code-block:: none

   module(load="omfwd")

   template(name="forwardFormat" type="list") {
       property(name="timestamp" dateFormat="rfc3339")
       constant(value=" ")
       property(name="hostname")
       constant(value=" ")
       property(name="syslogtag")
       property(name="msg")
       constant(value="\n")
   }

   ruleset(name="processRecipients") {
       # Assume $!recipients contains "sales,engineering,support"
       set $!recipient_array = split($!recipients, ",");

       # Iterate through each recipient and route accordingly
       foreach ($.recipient in $!recipient_array) do {
           if $.recipient == "sales" then {
               action(type="omfwd" target="sales-log.example.com" port="514"
                      template="forwardFormat")
           } else if $.recipient == "engineering" then {
               action(type="omfwd" target="eng-log.example.com" port="514"
                      template="forwardFormat")
           } else if $.recipient == "support" then {
               action(type="omfwd" target="support-log.example.com" port="514"
                      template="forwardFormat")
           }
       }
   }

   # Example: Extract recipients from a structured log field and process
   if $programname == "app-router" then {
       # Parse the message to extract recipients (assumes JSON input)
       set $!recipients = $!msg!notify;
       call processRecipients
   }

See Also
--------

- :doc:`field() <rs-field>` - Extract a single field from delimited data
- :doc:`parse_json() <rs-parse_json>` - Parse a JSON string
