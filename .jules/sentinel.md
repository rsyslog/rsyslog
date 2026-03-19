## 2026-03-18 - [Critical] Buffer overflow in str_split (plugins/mmdblookup/mmdblookup.c)
**Vulnerability:** Stack and Heap Buffer Overflows in string parsing logic where single characters were being replaced by multiple characters without buffer resizing.
**Learning:** When performing string modifications that expand the payload size, the destination buffer MUST be dynamically re-allocated. VLA (Variable Length Arrays) based on source length are fundamentally insecure for string expansions.
**Prevention:** Always allocate the maximum possible output buffer size `strlen(buf) * max_expansion_factor + 1` or realloc dynamically during the string modification loop.
