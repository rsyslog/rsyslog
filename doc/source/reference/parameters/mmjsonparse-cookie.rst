.. _param-mmjsonparse-cookie:
.. _mmjsonparse.parameter.input.cookie:

cookie
======

.. index::
   single: mmjsonparse; cookie
   single: cookie

.. summary-start

Defines the cookie string that must appear before the JSON content of a message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsonparse`.

:Name: cookie
:Scope: input
:Type: string
:Default: "@cee:"
:Required?: no
:Introduced: at least 6.6.0, possibly earlier

Description
-----------
Permits setting the cookie that must be present in front of the JSON part of
the message.

Most importantly, this can be set to the empty string ("") to not require any
cookie. In this case, leading spaces are permitted in front of the JSON. No
non-whitespace characters are permitted after the JSON. If such is required,
:doc:`../../configuration/modules/mmnormalize` must be used.

Input usage
-----------
.. _mmjsonparse.parameter.input.cookie-usage:

.. code-block:: rsyslog

   action(type="mmjsonparse" cookie="@cee:")

See also
--------
See also :doc:`../../configuration/modules/mmjsonparse`.
