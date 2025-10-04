.. _param-mmjsonparse-mode:
.. _mmjsonparse.parameter.mode:

mode
====

.. index::
   single: mmjsonparse; mode
   single: mode
   single: parsing mode

.. summary-start

Specifies the parsing mode for JSON content detection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsonparse`.

:Name: mode
:Scope: action
:Type: string
:Default: cookie
:Required?: no
:Introduced: 8.2506.0

Description
-----------
Sets the parsing mode to determine how JSON content is detected and processed.

- **cookie** (default): Legacy CEE cookie mode. Expects messages to begin with the configured cookie (default "@cee:") followed by JSON content. This preserves backward compatibility with existing configurations.
- **find-json**: Scans the message to locate the first valid top-level JSON object "{...}" regardless of its position. The module uses a tokenizer-aware scanner that respects quotes and escapes to find complete JSON objects.

Input usage
-----------
.. _mmjsonparse.parameter.mode-usage:

.. code-block:: rsyslog

   # Legacy CEE cookie mode (default)
   action(type="mmjsonparse" mode="cookie")
   
   # Find-JSON scanning mode
   action(type="mmjsonparse" mode="find-json")

See also
--------
See also the :doc:`main mmjsonparse module documentation
<../../configuration/modules/mmjsonparse>`.