import cpp

import semmle.code.cpp.valuenumbering.HashCons

from FunctionCall call, Assignment a
where
    call.getTarget().getName().regexpMatch(".*realloc")
	and a.getRValue() = (FunctionCall) call
	and hashCons(a.getLValue()) = hashCons(call.getArgument(0))

select call, "Hard Memory Leak found (Realloc fail will not free the memory chunk)", a.getLValue()
