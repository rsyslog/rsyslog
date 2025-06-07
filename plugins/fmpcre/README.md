# fmpcre - PCRE-based matching function

This optional RainerScript function module provides `pcre_match()`. It allows
checking whether a given string matches a PCRE regular expression.

## Build

Install the PCRE development library (e.g. `libpcre3-dev` on Debian/Ubuntu) and
enable the module during configure:

```bash
./configure --enable-fmpcre
make
```

## Usage

Load the module and call `pcre_match()` inside RainerScript:

```rsyslog
module(load="fmpcre")
set $.hit = pcre_match($msg, "^foo.*bar$");
```
