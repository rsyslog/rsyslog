RainerScript ABNF
=================

This is the formal definition of RainerScript, as supported by rsyslog
configuration. Please note that this currently is working document and
the actual implementation may be quite different.

The first glimpse of RainerScript will be available as part of rsyslog
3.12.0 expression support. However, the 3.12. series of rsyslog will
have a partial script implementaiton, which will not necessariy be
compatible with the later full implementation. So if you use it, be
prepared for some config file changes as RainerScript evolves.

C-like comments (/\* some comment \*/) are supported in all pure
RainerScript lines. However, legacy-mapped lines do not support them.
All lines support the hash mark "#" as a comment initiator. Everything
between the hash and the end of line is a comment (just like // in C++
and many other languages).

Formal Definition
-----------------

Below is the formal language definitionin ABNF (RFC 2234) format:

::

    ; all of this is a working document and may change! -- rgerhards, 2008-02-24script    := *stmtstmt      := (if_stmt / block / vardef / run_s / load_s)vardef    := "var" ["scope" = ("global" / "event")] block     := "begin" stmt "end"load_s    := "load" constraint ("module") modpath params ; load mod only if expr is truerun_s     := "run" constraint ("input") nameconstraint:= "if" expr ; constrains some one-time commandsmodpath   := exprparams    := ["params" *1param *("," param) "endparams"]param     := paramname) "=" exprparamname := [*(obqualifier ".") name]modpath:= ; path to module?line? := cfsysline / cflicfsysline:= BOL "$" *char EOL ; how to handle the first line? (no EOL in front!)BOL := ; Begin of Line - implicitely set on file beginning and after each EOLEOL := 0x0a ;LFif_stmt := "if" expr "then"old_filter:= BOL facility "." severity ; no whitespace allowed between BOL and facility!facility := "*" / "auth" / "authpriv" / "cron" / "daemon" / "kern" / "lpr" /  "mail" / "mark" / "news" / "security" / "syslog" / "user" / "uucp" /  "local0" .. "local7" / "mark" ; The keyword security should not be used anymore   ; mark is just internalseverity := TBD ; not really relevant in this context; and now the actual expressionexpr := e_and *("or" e_and)e_and := e_cmp *("and" e_cmp)e_cmp := val 0*1(cmp_op val)val := term *(("+" / "-" / "&") term)term := factor *(("*" / "/" / "%") factor)factor := ["not"] ["-"] terminalterminal := var / constant / function / ( "(" expr ")" )function := name "(" *("," expr) ")"var := "$" varnamevarname := msgvar / sysvarmsgvar := namesysvar := "$" namename := alpha *(alnum)constant := string / numberstring := simpstr / tplstr ; tplstr will be implemented in next phasesimpstr := "'" *char "'" ; use your imagination for char ;)tplstr := '"' template '"' ; not initially implementednumber := ["-"] 1*digit ; 0nn = octal, 0xnn = hex, nn = decimalcmp_op := "==" / "!=" / "<>" / "<" / ">" / "<=" / ">=" / "contains" / "contains_i" / "startswith" / "startswith_i"digit := %x30-39alpha := "a" ... "z" # all lettersalnum :* alpha / digit / "_" /"-" # "-" necessary to cover currently-existing message properties

Samples
-------

Some samples of RainerScript:

define function IsLinux
begin
    if $environ contains "linux" then return true else return false
end

load if IsLinux() 'imklog.so' params name='klog' endparams /\* load klog
under linux only \*/
run if IsLinux() input 'klog'
load 'ommysql.so'

if $message contains "error" then
  action
    type='ommysql.so', queue.mode='disk', queue.highwatermark = 300,
    action.dbname='events', action.dbuser='uid',
    [?action.template='templatename'?] or [?action.sql='insert into
table... values('&$facility&','&$severity&...?]
  endaction
... or ...

define action writeMySQL
    type='ommysql.so', queue.mode='disk', queue.highwatermark = 300,
    action.dbname='events', action.dbuser='uid',
    [?action.template='templatename'?] or [?action.sql='insert into
table... values(' & $facility & ','  & $severity &...?]
   endaction

if $message contains "error" then action writeMySQL

ALTERNATE APPROACH

define function IsLinux(
    if $environ contains "linux" then return true else return false
)

load if IsLinux() 'imklog.so' params name='klog' endparams /\* load klog
under linux only \*/
run if IsLinux() input 'klog'
load 'ommysql.so'

if $message contains "error" then
  action(
    type='ommysql.so', queue.mode='disk', queue.highwatermark = 300,
    action.dbname='events', action.dbuser='uid',
    [?action.template='templatename'?] or [?action.sql='insert into
table... values('&$facility&','&$severity&...?]
  )
... or ...

define action writeMySQL(
    type='ommysql.so', queue.mode='disk', queue.highwatermark = 300,
    action.dbname='events', action.dbuser='uid',
    [?action.template='templatename'?] or [?action.sql='insert into
table... values('&$facility&','&$severity&...?]
   )

if $message contains "error" then action
writeMySQL(action.dbname='differentDB')

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ]

Implementation
--------------

RainerScript will be implemented via a hand-crafted LL(1) parser. I was
tempted to use yacc, but it turned out the resulting code was not
thread-safe and as such did not fit within the context of rsyslog. Also,
limited error handling is not a real problem for us: if there is a
problem in parsing the configuration file, we stop processing. Guessing
what was meant and trying to recover would IMHO not be good choices for
something like a syslogd. [`manual index <manual.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
