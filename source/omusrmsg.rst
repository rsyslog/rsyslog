`back <rsyslog_conf_modules.html>`_

User Message Output Module
==========================

**Module Name:    omusrmsg**

**Author:**\ Rainer Gerhards <rgergards@adiscon.com>

**Description**:

The omusrmsg plug-in provides the core functionality for logging output
to a logged on user. It is a built-in module that does not need to be
loaded.

 

**Global Configuration Directives**:

-  **Template**\ [templateName]
    sets a new default template for file actions.

 

**Action specific Configuration Directives**:

-  **Users**\ string
    Must be a valid user name or root.

**Caveats/Known Bugs:**

-  None.

**Sample:**

The following command sends all critical syslog messages to a user and
to root.

Module (path="builtin:omusrmsg") \*.=crit action(type="omusrmsg"
Users="ExampleUser" Users="root" )

**Legacy Configuration Directives**:

No specific configuration directives available. See configuration sample
below for details on using the plugin.

**Legacy Sample:**

The following command sends all critical syslog messages to a user and
to root.

$ModLoad omusrmsg \*.=crit :omusrmsg:exampleuser & root

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
