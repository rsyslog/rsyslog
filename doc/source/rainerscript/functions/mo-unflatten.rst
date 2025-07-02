************
Unflatten
************

Purpose
=======

``<result> = unflatten(<source-tree>, <key-separator-character>);``

This function unflattens keys in a JSON object. It provides a way to expand dot-separated fields.

It allows for instance to produce this:
    ``{ "source": { "ip": "1.2.3.4", "port": 443 } }``

from this source data:
    ``{ "source.ip": "1.2.3.4", "source.port": 443 }``


Example
=======

Here is a sample use case:

.. code-block:: none

    module(load="fmunflatten")

    # Say you have the following tree, obtained for instance with mmnormalize.
    set $!source.ip = "1.2.3.4";
    set $!source.bytes = 3258;
    set $!source.geo.country_iso_code = "FR";
    set $!destination.ip = "4.3.2.1";

    # Now unflatten the keys in the $! tree.
    set $.unflatten = unflatten($!, ".");

    # You may do this to set back safely the result in $! because the function could
    # return a default dummy value of 0 (rsyslog number) if $! was not touched (it
    # would evaluate to an empty rsyslog string, which is not a JSON datatype).
    if (script_error() == 0) then {
        unset $!;
        set $! = $.unflatten;
        unset $.unflatten;
    }

An output of ``$!`` would give this, in pretty-print:

.. code-block:: none

    {
      "source": {
        "ip": "1.2.3.4",
        "bytes": 3258,
        "geo": {
          "country_iso_code": "FR"
        }
      },
      "destination": {
        "ip": "4.3.2.1"
      }
    }
