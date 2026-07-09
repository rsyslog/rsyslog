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
- ``controlCharacters`` – handle control characters: ``escape``,
  ``escape-octal``, ``space``, or ``drop``. ``escape`` keeps the legacy
  decimal ``#010`` style, while ``escape-octal`` uses the octal ``#012`` style
  used by receive-time control-character escaping.

  RainerScript example:

  .. code-block:: none

     template(name="octal-control" type="list") {
         property(name="msg" controlCharacters="escape-octal")
         constant(value="\n")
     }

  YAML example:

  .. code-block:: yaml

     templates:
       - name: octal-control
         type: list
         elements:
           - property:
               name: msg
               controlCharacters: escape-octal
           - constant:
               value: "\n"
- ``securePath`` – create safe paths for dynafile templates; ``drop`` or
  ``replace``
- ``format`` – field format. Supported values:

  - ``csv`` – generate CSV data
  - ``json`` – JSON-encode content without field header
  - ``jsonf`` – complete JSON field
  - ``jsonr`` – avoid double escaping while keeping JSON safe
  - ``jsonfr`` – combination of ``jsonf`` and ``jsonr``

- ``position.to`` – end position. Since 8.2302.0, negative values strip
  characters from the end. For example, ``position.from="2"
  position.to="-1"`` removes the first and last character. This is handy
  when removing surrounding characters such as brackets:

  .. code-block:: none

     property(name="msg" position.from="2" position.to="-1")

  With an input of ``[abc]`` the result is ``abc``. This is especially
  useful when parsing ``STRUCTURED-DATA`` fields.
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
- ``dataType`` – for ``jsonf`` format only; sets JSON data type. Rsyslog
  properties are strings, but some consuming systems require numbers or
  booleans. ``dataType`` controls how the ``jsonf`` field is emitted.
  Supported values:

  - ``number`` – emit numeric value, using 0 if empty
  - ``string`` – emit as string (default)
  - ``auto`` – use number if integer, otherwise string
  - ``bool`` – emit ``true`` unless value is empty or ``0``

  ``dataType`` does not create JSON arrays or objects from strings. If the
  incoming message text contains JSON such as ``["a","b"]``, parse it first
  with :ref:`mmjsonparse <ref-mmjsonparse>`. If an already extracted property
  contains JSON text, parse that string with :doc:`parse_json()
  <../../rainerscript/functions/rs-parse_json>`. Then render the parsed
  property with ``format="jsonf"``. Rsyslog intentionally does not provide
  ``dataType="array"`` for templates.

- ``onEmpty`` – for ``jsonf`` format only; handling of empty values:
  ``keep``, ``skip``, or ``null``
