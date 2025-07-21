Configuration Parameter Types
=============================
Configuration parameter values have different data types.
Unfortunately, the type currently must be guessed from the description
(consider contributing to the doc to help improve it). In general, the
following types are used:

- **numbers**

  The traditional integer format.  Numbers may include '.' and ','
  for readability. So you can for example specify either "1000" or
  "1,000" with the same result. Please note that rsyslogd simply
  *ignores* the punctuation. From it's point of view, "1,,0.0.,.,0"
  also has the value 1000.

- **sizes**

  Used for things like file size, main message queue sizes and the like.
  These are integers, but support modifier after the number part.
  For example, 1k means 1024. Supported are
  k(ilo), m(ega), g(iga), t(era), p(eta) and e(xa). Lower case letters
  refer to the traditional binary definition (e.g. 1m equals 1,048,576)
  whereas upper case letters refer to their new 1000-based definition (e.g
  1M equals 1,000,000).

- **complete line**

  A string consisting of multiple characters. This is relatively
  seldom used and sometimes looks confusing (rsyslog v7+ has a much better
  approach at these types of values).

- **single word**

  This is used when only a single word can be provided. A "single
  word" is a string without spaces in it. **No** quoting is necessary
  nor permitted (the quotes would become part of the word).

- **character**

  A single (printable) character. Must **not** be quoted.

- **boolean**

  The traditional boolean type, specified as "on" (1) or "off" (0).

Note that some other value types are occasionally used. However, the
majority of types is one of those listed above. The list is updated
as need arises and time permits.
