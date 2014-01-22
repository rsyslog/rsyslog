Functions
=========

RainerScript supports a currently quite limited set of functions:

-  getenv(str) - like the OS call, returns the value of the environment
   variable, if it exists. Returns an empty string if it does not exist.
-  strlen(str) - returns the length of the provided string
-  tolower(str) - converts the provided string into lowercase

The following example can be used to build a dynamic filter based on
some environment variable:

::

    if $msg contains getenv('TRIGGERVAR') then /path/to/errfile