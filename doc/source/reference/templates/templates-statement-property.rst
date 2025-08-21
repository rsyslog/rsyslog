.. _ref-templates-statement-property:

Property statement
==================

.. summary-start

Extracts and optionally transforms message properties.
Supports substring, case, regex, JSON formatting, and more.
.. summary-end

The property statement inserts property values. It can access any
property and offers numerous options to select portions of a property or
modify its text.

Parameters
----------

- ``name`` – property to access
- ``outname`` – output field name for structured outputs
- ``dateFormat`` – date format for date-related properties. See
  :doc:`property replacer <../../configuration/property_replacer>` for
  available formats. The property replacer documentation currently lists
  options for string templates. The names here omit the ``date-`` prefix
  (e.g. ``year`` rather than ``date-year``).

  To build custom formats, combine multiple property statements:

  .. code-block:: none

      property(name="timereported" dateFormat="year")
      constant(value="-")
      property(name="timereported" dateFormat="month")
      constant(value="-")
      property(name="timereported" dateFormat="day")

- ``date.inUTC`` – show date in UTC. Available since 8.18.0.
- ``caseConversion`` – convert text case; ``lower`` or ``upper``
- ``controlCharacters`` – handle control characters: ``escape``, ``space``,
  or ``drop``
- ``securePath`` – create safe paths for dynafile templates; ``drop`` or
  ``replace``
- ``format`` – field format. Supported values:

  - ``csv`` – generate CSV data
  - ``json`` – JSON-encode content without field header
  - ``jsonf`` – complete JSON field
  - ``jsonr`` – avoid double escaping while keeping JSON safe
  - ``jsonfr`` – combination of ``jsonf`` and ``jsonr``

- ``position.from`` – start position for substring extraction (1-based)
- ``position.to`` – end position. Since 8.2302.0, negative values strip
  characters from the end. For example, ``position.from="2"
  position.to="-1"`` removes the first and last character.
- ``position.relativeToEnd`` – interpret positions relative to end of
  string (since 7.3.10)
- ``fixedWidth`` – pad string with spaces up to ``position.to`` when
  shorter; ``on`` or ``off`` (default) (since 8.13.0)
- ``compressSpace`` – compress multiple spaces to one, applied after
  substring extraction (since 8.18.0)
- ``field.number`` – select this field match
- ``field.delimiter`` – decimal value of delimiter character
- ``regex.expression`` – regular expression
- ``regex.type`` – ``ERE`` or ``BRE``
- ``regex.noMatchMode`` – action when there is no match
- ``regex.match`` – match to use
- ``regex.subMatch`` – submatch to use
- ``dropLastLf`` – drop trailing LF
- ``mandatory`` – if ``on``, always emit field for structured outputs even
  when empty
- ``spIfNo1stSp`` – expert option for RFC3164 processing
- ``dataType`` – for ``jsonf`` format only; sets JSON data type. Supported
  values:

  - ``number`` – emit numeric value, using 0 if empty
  - ``string`` – emit as string (default)
  - ``auto`` – use number if integer, otherwise string
  - ``bool`` – emit ``true`` unless value is empty or ``0``

- ``onEmpty`` – for ``jsonf`` format only; handling of empty values:
  ``keep``, ``skip``, or ``null``

