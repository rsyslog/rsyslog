Number generator and counter module (mmsequence)
================================================

**Module Name:    mmsequence**

**Author:**\ Pavel Levshin <pavel@levshin.spb.ru>

**Status:**\ Non project-supported module - contact author or rsyslog
mailing list for questions

**This module is deprecated** in v8 and solely provided for backward
compatibility reasons. It was written as a work-around for missing
global variable support in v7. Global variables are available in v8,
and at some point in time this module will entirely be removed.

**Do not use this module for newly crafted config files.**
Use global variables instead.


**Available since**: 7.5.6

**Description**:

This module generates numeric sequences of different kinds. It can be
used to count messages up to a limit and to number them. It can generate
random numbers in a given range.

This module is implemented via the output module interface, so it is
called just as an action. The number generated is stored in a variable.

 

**Action Parameters**:

Note: parameter names are case-insensitive.

-  **mode** "random" or "instance" or "key"

   Specifies mode of the action. In "random" mode, the module generates
   uniformly distributed integer numbers in a range defined by "from"
   and "to".

   In "instance" mode, which is default, the action produces a counter
   in range [from, to). This counter is specific to this action
   instance.

   In "key" mode, the counter can be shared between multiple instances.
   This counter is identified by a name, which is defined with "key"
   parameter.

-  **from** [non-negative integer], default "0"

   Starting value for counters and lower margin for random generator.

-  **to** [positive integer], default "INT\_MAX"

   Upper margin for all sequences. Note that this margin is not
   inclusive. When next value for a counter is equal or greater than
   this parameter, the counter resets to the starting value.

-  **step** [non-negative integer], default "1"

   Increment for counters. If step is "0", it can be used to fetch
   current value without modification. The latter not applies to
   "random" mode. This is useful in "key" mode or to get constant values
   in "instance" mode.

-  **key** [word], default ""

   Name of the global counter which is used in this action.

-  **var** [word], default "$!mmsequence"

   Name of the variable where the number will be stored. Should start
   with "$".

**Sample**:

::

    # load balance
    Ruleset(
        name="logd"
        queue.workerthreads="5"
        ){

        Action(
            type="mmsequence"
            mode="instance"
            from="0"
            to="2"
            var="$.seq"
        )

        if $.seq == "0" then {
            Action(
                type="mmnormalize"
                userawmsg="on"
                rulebase="/etc/rsyslog.d/rules.rb"
            )
        } else {
            Action(
                type="mmnormalize"
                userawmsg="on"
                rulebase="/etc/rsyslog.d/rules.rb"
            )
        }

        # output logic here
    }
        # generate random numbers
        action(
            type="mmsequence"
            mode="random"
            to="100"
            var="$!rndz"
        )
        # count from 0 to 99
        action(
            type="mmsequence"
            mode="instance"
            to="100"
            var="$!cnt1"
        )
        # the same as before but the counter is global
        action(
            type="mmsequence"
            mode="key"
            key="key1"
            to="100"
            var="$!cnt2"
        )
        # count specific messages but place the counter in every message
        if $msg contains "txt" then
            action(
                type="mmsequence"
                mode="key"
                to="100"
                var="$!cnt3"
            )
        else
            action(
                type="mmsequence"
                mode="key"
                to="100"
                step="0"
                var="$!cnt3"
                key=""
            )

**Legacy Configuration Parameters**:

Note: parameter names are case-insensitive.

Not supported.

