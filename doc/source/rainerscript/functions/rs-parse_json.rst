************
parse_json()
************

Purpose
=======

parse_json(str, container)

Parses the json string ``str`` and places the resulting json object
into ``container`` where container can be any valid rsyslog variable.
Note that the **entire** ``str`` must be valid JSON for the function
to succeed. If there is trailing data after a valid JSON object/value,
it will be considered as an error.
Returns 0 on success and something otherwise if ``str`` does **not**
contain a valid, complete json string.


Example
=======

In the following example the json string is placed into the variable $!parsed.
The output is placed in variable $.ret

.. code-block:: none

   set $.ret = parse_json("{ \"c1\":\"data\" }", "\$!parsed");



