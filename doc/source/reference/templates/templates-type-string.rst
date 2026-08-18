.. _ref-templates-type-string:

String template type
====================

.. summary-start

Uses a single template string mixing constants and replacement variables.
Best suited for textual output with simple manipulation needs.
.. summary-end

String templates closely resemble the legacy ``$template`` statement.
They have a mandatory parameter ``string`` which holds the template
string to be applied. The string mixes constant text and replacement
variables processed by the :doc:`property replacer <../../configuration/property_replacer>`.

Example:

.. code-block:: none

   template(name="tpl3" type="string"
            string="%TIMESTAMP:::date-rfc3339% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\n"
           )

The text between percent signs is interpreted by the property replacer,
which reads message properties and applies options for formatting and
processing.

When a string template is used as an ``omfile`` ``dynaFile`` name,
escape each message-derived path component with ``secpath-replace``.
This is needed for fields such as ``HOSTNAME``, ``programname``,
``APP-NAME``, or variables populated from message content:

.. code-block:: none

   template(name="DynFile" type="string"
            string="/var/log/hosts/%HOSTNAME:::secpath-replace%/%programname:::secpath-replace%.log")

The escaping must be applied to the properties that form path
components, not to the whole path including fixed separators.
