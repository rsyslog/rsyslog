$ResetConfigVariables
---------------------

**Type:** global configuration directive

**Default:**

**Description:**

Resets all configuration variables to their default value. Any settings
made will not be applied to configuration lines following the
$ResetConfigVariables. This is a good method to make sure no
side-effects exists from previous directives. This directive has no
parameters.

**Sample:**

``$ResetConfigVariables``

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
