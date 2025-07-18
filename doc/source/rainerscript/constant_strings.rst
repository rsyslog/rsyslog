String Constants
================

String constants are necessary in any scripting language. They provide
values that are evaluated at startup and never change during rsyslog's
run.

Uses
----
String constants are necessary in many places: comparisons,
configuration parameter values and function arguments, to name a few
important ones.

In string constants, special characters are escaped by prepending a
backslash in front of them -- just in the same way this is done in the C
programming language or PHP.

If in doubt how to properly escape, use the `RainerScript String Escape
Online
Tool <http://www.rsyslog.com/rainerscript-constant-string-escaper/>`_.

Types
-----

Rsyslog provides different types of string constants, closely inspired
by the shell:

- single quotes

  Values are used unaltered, except for escape sequences, which are
  escaped.

- double quotes

  Right now, equivalent to single quotes, but $ signs need to be escaped.
  If not escaped, a syntax error will be generated and rsyslog startup
  be aborted in most cases.
  The idea is to support environment variables just like the shell does
  in later releases.

- backticks

  This was added in 8.33.0. The idea is to provide a useful subset of
  what the shell does. Right now, only the following is supported:

  - `echo $VARNAME` - It will evaluate the environment variable and use
    it as string constant.  If the variable is not found, an empty string
    is generated (this is **not** an error).

    Starting with 8.37.0, the `echo` case has been enhanced. It is now
    more along the lines of what bash does. It supports multiple
    environment variable expansions as well as constant text
    between them.

    An example:

    * env SOMEPATH is set to "/var/log/custompath"
    * config is: param=echo $SOMEPATH/myfile
    * param than is expanded to "/var/log/custompath/myfile"

    Note, however, that some common bash features are not supported.
    Environment variables are terminated once a character not belonging
    to the set ``[A-Za-z0-9_]`` is encountered or by the closing brace in
    ``${VAR}`` syntax. Constructs like ``$(pwd)`` remain unsupported. The idea
    of this parameter is not to provide a
    full-blown bash-equivalent, but provide some functionality that is
    usually considered useful for customizing rsyslog configuration with
    outside data.  For example, an expression like::

        `echo Log-$HOSTNAME!`

    expands to ``Log-<yourhost>!`` where ``$HOSTNAME`` is replaced and the
    exclamation mark follows as literal text. The braces form ``${HOSTNAME}``
    can also be used, e.g. `` `echo {${HOSTNAME}}` ``, which results in
    ``{<yourhost>}``. That said, we are still
    interested in extending the coverage if clear need and reasoning is
    provided.

  - `cat filename` - It will evaluate to the content of the given file.
    Only a single file name is supported. If the file is not readable,
    it will evaluate to an empty string.

  Any other construct will currently lead to an error message.
  Note that there must be exactly one space between "echo" or "cat" and
  the other parameter.

  Backticks are especially useful for configuration files that are
  auto-generated but need to contain a small set of special functionality.

  For an example of this in action, have a look at the rsyslog docker
  appliance available at
  https://github.com/rsyslog/rsyslog-docker/tree/master/appliance/alpine.
