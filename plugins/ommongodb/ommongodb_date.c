#include "config.h"

#include "ommongodb_date.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static int parseNDigits(const char **pos, int digits, int min, int max, int *out) {
    int val;
    int i;

    val = 0;
    for (i = 0; i < digits; ++i) {
        const char ch = **pos;
        if (ch < '0' || ch > '9') {
            return 0;
        }
        val = val * 10 + ch - '0';
        ++*pos;
    }
    if (val < min || val > max) {
        return 0;
    }
    *out = val;
    return 1;
}

static int consumeChar(const char **pos, char ch) {
    if (**pos != ch) {
        return 0;
    }
    ++*pos;
    return 1;
}

static int isLeapYear(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int daysInMonth(int year, int month) {
    static const int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return monthDays[month - 1];
}

static int64_t daysFromCivil(int year, int month, int day) {
    const int adjustedYear = year - (month <= 2);
    const int era = (adjustedYear >= 0 ? adjustedYear : adjustedYear - 399) / 400;
    const unsigned yoe = (unsigned)(adjustedYear - era * 400);
    const unsigned monthPrime = (unsigned)(month + (month > 2 ? -3 : 9));
    const unsigned doy = (153 * monthPrime + 2) / 5 + (unsigned)day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static int parseFractionMs(const char **pos, int *milliseconds) {
    int ms;
    int digits;

    *milliseconds = 0;
    if (**pos != '.') {
        return 1;
    }
    ++*pos;

    ms = 0;
    digits = 0;
    while (**pos >= '0' && **pos <= '9') {
        if (digits < 3) {
            ms = ms * 10 + **pos - '0';
        }
        ++digits;
        ++*pos;
    }
    if (digits == 0) {
        return 0;
    }
    while (digits < 3) {
        ms *= 10;
        ++digits;
    }

    *milliseconds = ms;
    return 1;
}

static int parseTimezoneOffsetMinutes(const char **pos, int *offsetMinutes) {
    int sign;
    int hours;
    int minutes;

    if (**pos == 'Z') {
        ++*pos;
        *offsetMinutes = 0;
        return 1;
    }
    if (strncmp(*pos, "UTC", 3) == 0) {
        *pos += 3;
        *offsetMinutes = 0;
        return 1;
    }
    if (strncmp(*pos, ":UTC", 4) == 0) {
        *pos += 4;
        *offsetMinutes = 0;
        return 1;
    }
    if (**pos != '+' && **pos != '-') {
        return 0;
    }

    sign = (**pos == '-') ? -1 : 1;
    ++*pos;
    if (!parseNDigits(pos, 2, 0, 23, &hours)) {
        return 0;
    }
    if (**pos == ':') {
        ++*pos;
    }
    if (!parseNDigits(pos, 2, 0, 59, &minutes)) {
        return 0;
    }

    *offsetMinutes = sign * (hours * 60 + minutes);
    return 1;
}

int ommongodbParseIsoDateMs(const char *date, int64_t *timestampMs) {
    const char *pos;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int milliseconds;
    int offsetMinutes;
    int64_t seconds;

    if (date == NULL || timestampMs == NULL) {
        return 0;
    }

    pos = date;
    if (!parseNDigits(&pos, 4, 0, 9999, &year) || !consumeChar(&pos, '-') || !parseNDigits(&pos, 2, 1, 12, &month) ||
        !consumeChar(&pos, '-') || !parseNDigits(&pos, 2, 1, 31, &day) || !consumeChar(&pos, 'T') ||
        !parseNDigits(&pos, 2, 0, 23, &hour) || !consumeChar(&pos, ':') || !parseNDigits(&pos, 2, 0, 59, &minute) ||
        !consumeChar(&pos, ':') || !parseNDigits(&pos, 2, 0, 59, &second) || !parseFractionMs(&pos, &milliseconds) ||
        !parseTimezoneOffsetMinutes(&pos, &offsetMinutes) || *pos != '\0') {
        return 0;
    }

    if (day > daysInMonth(year, month)) {
        return 0;
    }

    seconds = daysFromCivil(year, month, day) * 86400 + (int64_t)hour * 3600 + (int64_t)minute * 60 + second;
    seconds -= (int64_t)offsetMinutes * 60;
    if (seconds > (INT64_MAX - milliseconds) / 1000 || seconds < (INT64_MIN + milliseconds) / 1000) {
        return 0;
    }

    *timestampMs = seconds * 1000 + milliseconds;
    return 1;
}
