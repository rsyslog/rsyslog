.. _mo-fmpcre:

.. meta::
   :description: PCRE-based matching function module for RainerScript.
   :keywords: rsyslog, rainerscript, module, fmpcre, pcre_match, regex, pcre

.. summary-start

The fmpcre module adds the pcre_match() function for PCRE-style regular
expression matching in RainerScript.

.. summary-end

.. index:: ! fmpcre

**********************
fmpcre (PCRE matching)
**********************

Purpose
=======

The fmpcre function module provides `pcre_match()` for matching strings with
PCRE-style regular expressions. Unlike `re_match()`, which uses POSIX ERE,
`pcre_match()` supports PCRE syntax such as lookarounds and inline modifiers.

Requirements
============

This module is optional and requires the PCRE development library to be
installed at build time. Enable it during configure:

.. code-block:: none

   ./configure --enable-fmpcre

Example
=======

Load the module before calling the function:

.. code-block:: rsyslog

   module(load="fmpcre")
   set $.hit = pcre_match($msg, "^foo.*bar$");

Function
========

pcre_match(expr, re)

Returns 1 when *expr* matches the PCRE pattern *re*, otherwise 0. The *re*
argument must be a constant string so the expression can be compiled during
configuration parsing.

.. note::

   PCRE matching can be more expensive than simple string operations.
   Consider cheaper filters before using complex expressions.

.. seealso::

   :doc:`re_match()<rs-re_match>`
   :doc:`re_match_i()<rs-re_match_i>`
