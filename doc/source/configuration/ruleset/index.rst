Ruleset-Specific Legacy Configuration Statements
================================================
These statements can be used to set ruleset parameters. To set
these parameters, first use *$Ruleset*, **then** use the other
configuration directives. Please keep in mind
that each ruleset has a *main* queue. To specify parameter for these
ruleset (main) queues, use the main queue configuration directives.

.. toctree::
   :glob:

   *rule*

-  **$Ruleset** *name* - starts a new ruleset or switches back to one
   already defined. All following actions belong to that new rule set.
   the *name* does not yet exist, it is created. To switch back to
   rsyslog's default ruleset, specify "RSYSLOG\_DefaultRuleset") as the
   name. All following actions belong to that new rule set. It is
   advised to also read our paper on
   :doc:`using multiple rule sets in rsyslog <../../concepts/multi_ruleset>`.
