.. _ref-templates-options:

Template options
================

.. summary-start

Global modifiers applied to a template.
Include SQL and JSON helpers and case sensitivity control.
.. summary-end

Template options influence the whole template and are specified as
parameters of the ``template()`` object. They are distinct from property
options which apply only to individual properties.

Available options (case-insensitive):

``option.sql``
  Format string for MariaDB/MySQL. Replaces single quotes (``'``) and
  backslashes (``\\``) by escaped counterparts (``\\'`` and ``\\\\``).
  MySQL must run with ``NO_BACKSLASH_ESCAPES`` turned off.

``option.stdsql``
  Format string for standards-compliant SQL servers. Replaces single
  quotes by doubled quotes (``''``). Use this with MySQL when
  ``NO_BACKSLASH_ESCAPES`` is enabled.

``option.json``
  Escape data suitable for JSON.

``option.jsonf``
  Render the template as a JSON object, adding braces and commas between
  elements.

``option.jsonftree``
  Render as JSON like ``option.jsonf`` but also interpret dotted
  ``outname`` segments (for example ``event.dataset.name``) as nested
  objects.  When this option is not set, dotted names remain flat strings
  in the output to preserve legacy behaviour.

``option.caseSensitive``
  Treat property names as case sensitive. Normally names are converted to
  lowercase at definition time. Enable this if JSON (``$!*``), local
  (``!.``), or global (``$!``) properties contain uppercase letters.

Options ``option.sql``, ``option.stdsql``, ``option.json``, ``option.jsonf``,
and ``option.jsonftree`` are mutually exclusive.

Either ``option.sql`` or ``option.stdsql`` must be specified when writing
into a database to guard against SQL injection. The database writer checks
for the presence of one of these options and refuses to run otherwise.

These options can also be useful when generating files intended for later
import into a database. Do not enable them without need as they introduce
extra processing overhead.
