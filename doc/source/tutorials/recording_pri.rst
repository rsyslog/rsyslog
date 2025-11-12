Recording the Priority of Syslog Messages
=========================================

*Written by* `Rainer Gerhards <https://rainer.gerhards.net/>`_ *(2007-06-18)*

Abstract
--------

**The so-called priority (PRI) is very important in syslog messages,
because almost all filtering in syslog.conf is based on it.** However,
many syslogds (including the Linux stock sysklogd) do not provide a way
to record that value. In this article, I'll give a brief overview of how
PRI can be written to a log file.

Background
----------

The PRI value is a combination of so-called severity and facility. The
facility indicates where the message originated from (e.g. kernel, mail
subsystem) while the severity provides a glimpse of how important the
message might be (e.g. error or informational). Be careful with these
values: they are in no way consistent across applications (especially
severity). However, they still form the basis of most filtering in
syslog.conf. For example, the directive (aka "selector line)"
historically looked like ``mail.* /var/log/mail.log``. Rainerscript
expresses the same idea more explicitly via the
:doc:`prifilt() <../rainerscript/functions/rs-prifilt>` function:

.. code-block:: rsyslog

   if prifilt("mail.*") then {
       action(type="omfile" file="/var/log/mail.log")
   }

Messages with the mail facility are stored to ``/var/log/mail.log``, no
matter which severity indicator they have (that is telling us the
asterisk). If you set up complex conditions, it can be annoying to find
out which PRI value a specific syslog message has. Most stock syslogds
do not provide any way to record them.

How is it done?
---------------

With `rsyslog <http://www.rsyslog.com/>`_, PRI recording is simple. All
you need is the correct template. Even if you do not use rsyslog on a
regular basis, it might be a handy tool for finding out the priority.

Rsyslog provides a flexible system to specify the output formats. Modern
Rainerscript configurations prefer list templates: each template is
composed of ``property()`` and ``constant()`` statements that are
expanded by the :doc:`property replacer <../configuration/property_replacer>`
before the final text is emitted. A template with the traditional syslog
format looks as follows:

.. code-block:: rsyslog

   template(
       name="TraditionalFormat"
       type="list"
   ) {
       property(name="timegenerated" dateFormat="rfc3164")
       constant(value=" ")
       property(name="hostname")
       constant(value=" ")
       property(name="syslogtag")
       property(name="msg" dropLastLf="on")
       constant(value="\n")
   }

The :doc:`../configuration/templates` documentation walks through the
available template types and modifiers in more detail.

Each ``property()`` statement pulls in a message property (for example,
``msg`` or ``hostname``) and can apply :doc:`property replacer
<../configuration/property_replacer>` options such as ``dropLastLf``. The
``constant()`` statements inject literal text such as spaces or newline
characters. Rsyslog concatenates the list entries in the order they are
defined.

Thankfully, rsyslog provides message properties for the priority. These
are called "PRI", "syslogfacility" and "syslogpriority" (case is
important!). They are numerical values. Starting with rsyslog 1.13.4,
there is also a property "pri-text", which contains the priority in
friendly text format (e.g. "local0.err<133>"). For the rest of this
article, I assume that you run version 1.13.4 or higher.

Recording the priority is now a simple matter of adding the respective
field to the template. It now looks like this:

.. code-block:: rsyslog

   template(
       name="TraditionalFormatWithPRI"
       type="list"
   ) {
       property(name="pri-text")
       constant(value=": ")
       property(name="timegenerated" dateFormat="rfc3164")
       constant(value=" ")
       property(name="hostname")
       constant(value=" ")
       property(name="syslogtag")
       property(name="msg" dropLastLf="on")
       constant(value="\n")
   }

Now we have the right template - but how to write it to a file? You
probably have a line like this in your syslog.conf:

.. code-block:: rsyslog

   action(
       type="omfile"
       file="/var/log/messages.log"
       template="TraditionalFormatWithPRI"
   )

That's all you need to do. There is one common pitfall: you need to
define the template before you use it in an action. Otherwise, you will
receive an error.

Once you have applied the changes, you need to restart rsyslogd. It will
then pick the new configuration.

What if I do not want rsyslogd to be the standard syslogd?
----------------------------------------------------------

If you do not want to switch to rsyslog, you can still use it as a setup
aid. A little bit of configuration is required.

#. Download, make and install rsyslog.
#. Copy your existing configuration to a test file (for example,
   ``/etc/rsyslog-pri.conf``).
#. Add the template and action described above to it; select the file
   that should use it.
#. Stop your regular syslog daemon for the time being.
#. Run rsyslogd (you may even do this interactively by calling it with
   the ``-n`` additional option from a shell).
#. Stop rsyslogd (press ctrl-c when running interactively).
#. Restart your regular syslogd.

That's it - you can now review the priorities.

Some Sample Data
----------------

Below is some sample data created with the template specified above.
Note the priority recording at the start of each line.

::

  kern.info<6>: Jun 15 18:10:38 host kernel: PCI: Sharing IRQ 11 with 00:04.0
  kern.info<6>: Jun 15 18:10:38 host kernel: PCI: Sharing IRQ 11 with 01:00.0
  kern.warn<4>: Jun 15 18:10:38 host kernel: Yenta IRQ list 06b8, PCI irq11
  kern.warn<4>: Jun 15 18:10:38 host kernel: Socket status: 30000006
  kern.warn<4>: Jun 15 18:10:38 host kernel: Yenta IRQ list 06b8, PCI irq11
  kern.warn<4>: Jun 15 18:10:38 host kernel: Socket status: 30000010
  kern.info<6>: Jun 15 18:10:38 host kernel: cs: IO port probe 0x0c00-0x0cff: clean.
  kern.info<6>: Jun 15 18:10:38 host kernel: cs: IO port probe 0x0100-0x04ff: excluding 0x100-0x107 0x378-0x37f 0x4d0-0x4d7
  kern.info<6>: Jun 15 18:10:38 host kernel: cs: IO port probe 0x0a00-0x0aff: clean.
  local7.notice<189>: Jun 15 18:17:24 host dd: 1+0 records out
  local7.notice<189>: Jun 15 18:17:24 host random: Saving random seed: succeeded
  local7.notice<189>: Jun 15 18:17:25 host portmap: portmap shutdown succeeded
  local7.notice<189>: Jun 15 18:17:25 host network: Shutting down interface eth1: succeeded
  local7.notice<189>: Jun 15 18:17:25 host network: Shutting down loopback interface: succeeded
  local7.notice<189>: Jun 15 18:17:25 host pcmcia: Shutting down PCMCIA services: cardmgr
  user.notice<13>: Jun 15 18:17:25 host /etc/hotplug/net.agent: NET unregister event not supported
  local7.notice<189>: Jun 15 18:17:27 host pcmcia: modules.
  local7.notice<189>: Jun 15 18:17:29 host rc: Stopping pcmcia: succeeded
  local7.notice<189>: Jun 15 18:17:30 host rc: Starting killall: succeeded
  syslog.info<46>: Jun 15 18:17:33 host [origin software="rsyslogd" swVersion="1.13.3" x-pid="2464"] exiting on signal 15.
  syslog.info<46>: Jun 18 10:55:47 host [origin software="rsyslogd" swVersion="1.13.3" x-pid="2367"][x-configInfo udpReception="Yes" udpPort="514" tcpReception="Yes" tcpPort="1470"] restart
  user.notice<13>: Jun 18 10:55:50 host rger: test
  syslog.info<46>: Jun 18 10:55:52 host [origin software="rsyslogd" swVersion="1.13.3" x-pid="2367"] exiting on signal 2.``
