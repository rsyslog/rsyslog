.. _ref-templates-legacy:

Legacy ``$template`` statement
==============================

.. summary-start

Historic syntax supporting only string templates.
Kept for compatibility with older configurations.
.. summary-end

Legacy format provides limited functionality but is still frequently
encountered. Only string templates are supported with the syntax:

.. code-block:: none

   $template myname,<string-template>

Here *myname* is the template name (similar to ``name="myname"`` in the
modern format) and ``<string-template>`` is the same as the ``string``
parameter in modern templates.

Example modern template:

.. code-block:: none

   template(name="tpl3" type="string"
            string="%TIMESTAMP:::date-rfc3339% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\n"
           )

Equivalent legacy statement:

.. code-block:: none

   $template tpl3,"%TIMESTAMP:::date-rfc3339% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\n"

