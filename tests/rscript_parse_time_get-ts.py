#!/usr/bin/env python
# Added 2017-11-05 by Stephen Workman, released under ASL 2.0

import re
import sys

from datetime import datetime

err = 0

JAN = 1; FEB =  2; MAR =  3; APR =  4
MAY = 5; JUN =  6; JUL =  7; AUG =  8
SEP = 9; OCT = 10; NOV = 11; DEC = 12

def do_test(expr, val):
    global err

    result = eval(expr)

    if result != val:
        print "Error: %s. Expected %4d, got %4d!" % (expr, val, result)
        err += 1

def estimate_year(cy, cm, im):
	im += 12

	if (im - cm) == 1:
		if cm == 12 and im == 13:
			return cy + 1

	if (im - cm) > 13:
		return cy - 1

	return cy;

def self_test():

    do_test("estimate_year(2017, DEC, JAN)", 2018)

    do_test("estimate_year(2017, NOV, DEC)", 2017)
    do_test("estimate_year(2017, OCT, NOV)", 2017)
    do_test("estimate_year(2017, SEP, OCT)", 2017)
    do_test("estimate_year(2017, AUG, SEP)", 2017)

    do_test("estimate_year(2017, NOV, JAN)", 2017)
    do_test("estimate_year(2017, NOV, FEB)", 2017)
    do_test("estimate_year(2017, AUG, OCT)", 2016)
    do_test("estimate_year(2017, AUG, MAR)", 2017)
    do_test("estimate_year(2017, APR, JUL)", 2016)

    do_test("estimate_year(2017, AUG, JAN)", 2017)
    do_test("estimate_year(2017, APR, FEB)", 2017)

    do_test("estimate_year(2017, JAN, DEC)", 2016)
    do_test("estimate_year(2017, JAN, FEB)", 2017)
    do_test("estimate_year(2017, JAN, MAR)", 2016)

def get_total_seconds(dt):
    # datetime.total_seconds() wasn't added until
    # Python 2.7, which CentOS 6 doesn't have.

    if hasattr(datetime, "total_seconds"):
        return time.total_seconds
    return dt.seconds + dt.days * 24 * 3600


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print "Invalid number of arguments!"
        sys.exit(1)

    if sys.argv[1] == "selftest":
        self_test()
        sys.exit(err)

    months = [None, "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]

    current_datetime = datetime.utcnow()
    incoming_datetime = sys.argv[1]

    incoming_month = re.search(r"^([^ ]+) ", incoming_datetime).group(1)
    incoming_month = months.index(incoming_month)

    estimated_year = estimate_year(
        current_datetime.year,
        current_datetime.month,
        incoming_month
    )

    calculated_datetime = datetime.strptime("%s %d" % (incoming_datetime, estimated_year), "%b %d %H:%M:%S %Y")

    print int( get_total_seconds(calculated_datetime - datetime(1970,1,1)) )
