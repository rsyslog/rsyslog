Provides the name of a :doc:`ratelimit object </rainerscript/ratelimit>`
whose settings should be applied. When this parameter is set, inline
``ratelimit.interval``/``ratelimit.burst`` options must be omitted. The
shared object supplies the interval, burst and (where supported) severity
threshold used by this component.

Use this parameter to keep rate limiting consistent across multiple
inputs or actions. The referenced object must be declared earlier in the
configuration.
