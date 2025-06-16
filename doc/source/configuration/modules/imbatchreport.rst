****************************************
imbatchreport: Batch report input module
****************************************

================  ==============================================================
**Module Name:**  **imbatchreport**
**Authors:**      Jean-Philippe Hilaire <jean-philippe.hilaire@pmu.fr> & Philippe Duveau <philippe.duveau@free.fr>
================  ==============================================================


Purpose
=======

This module allows rsyslog to manage batch reports.

Batch are programs launched successively to process a large amount of 
information. These programs are organized in stages with passing conditions. 
The batch ends with a global execution summary. Each Batch produces a single 
result file usually named with the name of the batch and its date of execution.

Those files have sense only when they are complete in one log. When the file is
collected it becomes useless and, as a statefile, should be deleted or renamed.

This module handle those characteristics :

- reads the complete file,

- extracts the structured data from the file (see managing structured data),

- transmit the message to output module(s),

- action is applied to the file to flag it as treated. Two different actions can be applied: delete or rename the file.

If the file is too large to be handled in the message size defined by rsyslog,
the file is renamed as a "rejected file". See \$maxMessageSize

**Managing structured data**

As part of the batch summary, the structure data can be provided in the batch
report file as the last part of the file. 

The last non-space char has to be a closing brace ']' then all chars between
this char up to the closest opening brace '[' are computed as structured data.

All the structured data has to be contained in the last 150 chars of the file.

In general, structured data should contain the batch name (program) and the 
start timestamp. Those two values can be extract to fill rsyslog message 
attributes.

Compile
=======

To successfully compile imbatchreport module.

    ./configure --enable-imbatchreport ...

Configuration Parameters
========================

Action Parameters
-----------------

Reports
^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "yes", "glob definition",   

Glob definition used to identify reports to manage.

Tag
^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "yes", ,"none"

The tag to be assigned to messages read from this file. If you would like to
see the colon after the tag, you need to include it when you assign a tag
value, like so: ``tag="myTagValue:"``.

Facility
^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "facility\|number", "local0" 

The syslog facility to be assigned to messages read from this file. Can be
specified in textual form (e.g. ``local0``, ``local1``, ...) or as numbers (e.g.
16 for ``local0``). Textual form is suggested.

Severity
^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "severity\|number", "notice"

The syslog severity to be assigned to lines read. Can be specified
in textual   form (e.g. ``info``, ``warning``, ...) or as numbers (e.g. 6
for ``info``). Textual form is suggested.

DeduplicateSpaces
^^^^^^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "binary", "no", "", "on"

The parameter modify the way consecutive spaces like chars are managed.
When it is set to "on", consecutive spaces like chars are reduced to a single one
and trailing space like chars are suppressed. 

Delete
^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "<regex> <reject>", 

This parameter informs the module to delete the report to flag it as treated. 
If the file is too large (or failed to be removed) it is renamed using the
<regex> to identify part of the file name that has to be replaced it by 
<reject>. See Examples

Rename
^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", "<regex> <sent> <reject>", 

This parameter informs the module to rename the report to flag it as treated.
The file is renamed using the <regex> to identify part of the file name that 
has to be replaced it:

- by <rename> if the file was successfully treated,

- by <reject> if the file is too large to be sent.

See #Examples

Programkey
^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", , 

The attribute in structured data which contains the rsyslog APPNAME.
This attribute has to be a String between double quotes ("). 

Timestampkey
^^^^^^^^^^^^

.. csv-table::
  :header: "type", "mandatory", "format", "default"
  :widths: auto
  :class: parameter-table

  "string", "no", , 

The attribute in structured data which contains the rsyslog TIMESTAMP.
This attribute has to be a Number (Unix TimeStamp). 

Examples
========

The example show the delete action. All files corresponding to 
"/test/\*.ok" will be treated as batch reports and will be deleted
on success or renamed from <file>.ok to <file>.rejected in other
cases.

.. code-block:: none

  module(load="imbatchreport")
  input(type="imbatchreport" reports="/test/\*.ok"
        ruleset="myruleset" tag="batch"
        delete=".ok$ .rejected"
        programkey="SHELL" timestampkey="START"
     )

The example show the rename action. All files corresponding to 
"/test/\*.ok" will be treated as batch reports and will be renamed
from <file>.ok to <file>.sent on success or 
renamed from <file>.ok to <file>.rejected in other cases.

.. code-block:: none

  module(load="imbatchreport")
  input(type="imbatchreport" reports="/test/\*.ok"
        ruleset="myruleset" tag="batch"
        rename=".ok$ .sent .rejected"
        programkey="SHELL" timestampkey="START"
     )
