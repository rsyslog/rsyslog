**************
get_property()
**************

Purpose
========

get_property(rsyslog_variable, key_str)

Provides ability to get a rsyslog variable or property using dynamically evaluated parameters.
The first parameter is a valid rsyslog variable or property, the second parameter is a key string, or index value.


Example
========

In the following example, a json string is parsed using parse_json(), and placed into the variable ``$!parsed``.
The get_property function is then used to get property fields from ``$!parsed``.

.. code-block:: none

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
      $.index = 1;
      # set $.ret = { "a": 9, "b": 3}
      set $.ret = get_property($!parsed!offsets, $.index);

      # set $.ret = true;
      set $.key = "boolval";
      set $.ret = get_property($!parsed, $.key);

      # null values are evaluated to empty string
      # thus below statement will set $.ret to empty string
      set $.ret = get_property($!parsed, "nullval");

      # evaluates to 1234567890
      set $.key = "int64val";
      set $.ret = get_property($!parsed, $.key);

      # using a key variable, evaluates to 12345.67890
      set $key = "doubleval";
      set $.ret = get_property($!parsed, $key);
   }

   # example of dynamically building key value
   set $.foo!barval = 3;
   # set $.bar = 3;
   set $.bar = get_property($.foo, "bar" & "val");
