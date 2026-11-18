.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Read rsyslog variables and properties using dynamic object keys or array indexes.
   :keywords: rsyslog, RainerScript, get_property, dynamic keys, array indexes

**************
get_property()
**************

Purpose
========

get_property(rsyslog_variable, key_str)

.. summary-start

Provides ability to get a rsyslog variable or property using dynamically evaluated parameters.

.. summary-end

The first parameter is a valid rsyslog variable or property, the second parameter is a key string, or index value.


Example
========

In the following example, a json string is parsed using parse_json(), and placed into the variable ``$!parsed``.
The get_property function is then used to get property fields from ``$!parsed``.
``reset`` replaces each previous result in ``$.ret``, including an object value.

.. code-block:: none
   :caption: RainerScript configuration

   set $.ret = parse_json("{\"offsets\": [ { \"a\": 9, \"b\": 0 },\
                                           { \"a\": 9, \"b\": 3 } ],\
                                           \"boolval\": true,\
                                           \"int64val\": 1234567890,\
                                           \"nullval\": null,\
                                           \"doubleval\": 12345.67890 }", "\$!parsed");

   # get different fields from $!parsed here
   if $.ret == 0 then {
      # dynamically evaluate different fields in $!parsed

      # do dynamic indexing into array
      set $.index = 1;
      # $.ret becomes { "a": 9, "b": 3}
      reset $.ret = get_property($!parsed!offsets, $.index);

      # $.ret becomes true
      set $.key = "boolval";
      reset $.ret = get_property($!parsed, $.key);

      # null values are evaluated to empty string
      # thus the statement below resets $.ret to an empty string
      reset $.ret = get_property($!parsed, "nullval");

      # evaluates to 1234567890
      set $.key = "int64val";
      reset $.ret = get_property($!parsed, $.key);

      # using a key variable, evaluates to 12345.6789
      set $.key = "doubleval";
      reset $.ret = get_property($!parsed, $.key);
   }

   # example of dynamically building key value
   set $.foo!barval = 3;
   # $.bar becomes 3
   set $.bar = get_property($.foo, "bar" & "val");
