************
Faup
************


Description
===========

These new functions allow anybody to parse any variable containing URLs, hostnames, DNS queries and such to extract:
 - the *scheme*
 - the *credentials* (if present)
 - the *tld* (with support for second-level TLDs)
 - the *domain* (with and without the tld)
 - the *subdomain*
 - the *full hostname*
 - the *port* (if present)
 - the *resource* path (if present)
 - the *query string* parameters (if present)
 - the *fragment* (if present)

HowTo
-----
The module functions are fairly simple to use, and are divided in 2 classes:
* `faup()` allows to parse the entire URL and return all parts in a complete JSON
* `faup_<field>()` allows to parse the entire URL, but only returns the (potential) value of the requested field as string

Examples
^^^^^^^^
Using the `faup()` function
"""""""""""""""""""""""""""""""
The `faup()` is the simplest function to use, simply provide a value or variable (any type) as the only parameter, and the function returns a json object containing every element of the URL.

*example code:*

.. code-block:: none

    set $!url = "https://user:pass@www.rsyslog.com:443/doc/v8-stable/rainerscript/functions/mo-faup.html?param=value#faup";
    set $.faup = faup($!url);


*$.faup will contain:*

.. code-block:: none

    {
      "scheme": "https",
      "credential": "user:pass",
      "subdomain": "www",
      "domain": "rsyslog.com",
      "domain_without_tld": "rsyslog",
      "host": "www.rsyslog.com",
      "tld": "com",
      "port": "443",
      "resource_path": "\/doc\/v8-stable\/rainerscript\/functions\/mo-ffaup.html",
      "query_string": "?param=value",
      "fragment": "#faup"
    }

.. note::

    This is a classic rsyslog variable, and you can access every sub-key with `$.faup!domain`, `$.faup!resource_path`, etc...


Using the `faup_<field>()` functions
""""""""""""""""""""""""""""""""""""""""
Using the field functions is even simpler: for each field returned by the `faup()` function, there exists a corresponding function to get only that one field.

For example, if the goal is to recover the domain without the tld, the example above could be modified as follows:

*example code:*

.. code-block:: none

    set $!url = "https://user:pass@www.rsyslog.com:443/doc/v8-stable/rainerscript/functions/mo-faup.html?param=value#faup";
    set $.faup = faup_domain_without_tld($!url);

*$.faup will contain:*

.. code-block:: none

    rsyslog

.. note::

    The returned value is no longer a json object, but a simple string


Requirements
============
This module relies on the `faup <https://github.com/stricaud/faup>`_ library.

The library should be compiled (see link for instructions on how to compile) and installed on build and runtime machines.

.. warning::

    Even if faup is statically compiled to rsyslog, the library still needs an additional file to work properly: the mozilla.tlds stored by the libfaup library in /usr/local/share/faup. It permits to properly match second-level TLDs and allow URLs such as www.rsyslog.co.uk to be correctly parsed into \<rsyslog:domain\>.\<co.uk:tld\> and not \<rsyslog:subdomain\>.\<co:domain\>.\<uk:tld\>


Motivations
===========
Those functions are the answer to a growing problem encountered in Rsyslog when using modules to enrich logs : some mechanics (like lookup tables or external module calls) require "strict" URL/hostname formats that are often not formatted correctly, resulting in lookup failures/misses.

This ensures getting stable inputs to provide to lookups/modules to enrich logs.