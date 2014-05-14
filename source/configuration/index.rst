Configuration
=============

**Rsyslogd is configured via the rsyslog.conf file**, typically found in
/etc. By default, rsyslogd reads the file /etc/rsyslog.conf. This may be
changed by a command line option.

All configuration directives need to be specified on a line by their own
and must start with a dollar-sign.

Configuration directive values have different types of parameters.
Unfortunately, the type must currently be guessed from the description
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
  refer to the traditional binary defintion (e.g. 1m equals 1,048,576)
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

**Important:** when you change the configuration, remember to restart
rsyslogd, because otherwise the newly added configurations settings
will not be loaded by the running instance.


.. toctree::
   :maxdepth: 2

   basic_structure
   templates
   properties
   property_replacer
   filters
   ../rainerscript/index
   expression
   actions
   droppriv
   examples
   modules/index
   action/index
   ruleset/index
   global/index
   output_channels
   ipv6

`Configuration file examples can be found in the rsyslog
wiki <http://wiki.rsyslog.com/index.php/Configuration_Samples>`_. Also
keep the `rsyslog config
snippets <http://www.rsyslog.com/config-snippets/>`_ on your mind. These
are ready-to-use real building blocks for rsyslog configuration.

There is also one sample file provided together with the documentation
set. If you do not like to read, be sure to have at least a quick look
at :download:`rsyslog-example.conf <rsyslog-example.conf>`.

While rsyslogd contains enhancements over standard syslogd, efforts have
been made to keep the configuration file as compatible as possible.
While, for obvious reasons, :doc:`enhanced features <../features>` require
a different config file syntax, rsyslogd should be able to work with a
standard syslog.conf file. This is especially useful while you are
migrating from syslogd to rsyslogd.
