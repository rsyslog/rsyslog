.. _param-mmjsonparse-allow_trailing:
.. _mmjsonparse.parameter.allow_trailing:

allow_trailing
==============

.. index::
   single: mmjsonparse; allow_trailing
   single: allow_trailing
   single: trailing data

.. summary-start

Whether to allow non-whitespace data after the JSON object in find-json mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsonparse`.

:Name: allow_trailing
:Scope: action
:Type: boolean
:Default: on
:Required?: no
:Introduced: 8.2506.0

Description
-----------
Controls whether trailing non-whitespace characters are permitted after the parsed JSON object in find-json mode.

- **on** (default): Allow trailing data after the JSON object. The JSON portion will be parsed and the trailing content ignored.
- **off**: Reject messages with trailing non-whitespace content after the JSON object. Such messages will be treated as non-JSON.

Whitespace characters (spaces, tabs, newlines) are always allowed after the JSON object regardless of this setting.

This parameter is only effective when mode="find-json".

Input usage
-----------
.. _mmjsonparse.parameter.allow_trailing-usage:

.. code-block:: rsyslog

   # Allow trailing data (default)
   action(type="mmjsonparse" mode="find-json" allow_trailing="on")
   
   # Reject messages with trailing data
   action(type="mmjsonparse" mode="find-json" allow_trailing="off")

See also
--------
See also the :doc:`main mmjsonparse module documentation
<../../configuration/modules/mmjsonparse>`.