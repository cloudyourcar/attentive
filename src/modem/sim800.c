#include <attentive/cellular.h>

#include <stdio.h>
#include <string.h>


struct cellular_sim800 {
    struct cellular dev;
};


static int sim800_op_imei(struct cellular *modem, char *buf, size_t len)
{
    char fmt[16];
    if (snprintf(fmt, sizeof(fmt), "%%[0-9]%ds", (int) len) >= (int) sizeof(fmt)) {
        errno = ENOSPC;
        return -1;
    }

    at_set_timeout(modem->at, 1);
    const char *response = at_command(modem->at, "AT+CGSN");
    at_simple_scanf(response, fmt, buf);

    return 0;
}

static int sim800_op_iccid(struct cellular *modem, char *buf, size_t len)
{
    char fmt[16];
    if (snprintf(fmt, sizeof(fmt), "%%[0-9]%ds", (int) len) >= (int) sizeof(fmt)) {
        errno = ENOSPC;
        return -1;
    }

    at_set_timeout(modem->at, 5);
    const char *response = at_command(modem->at, "AT+CCID");
    at_simple_scanf(response, fmt, buf);

    return 0;
}

static int sim800_op_gettime(struct cellular *modem, struct timespec *ts)
{
    struct tm tm;

    at_set_timeout(modem->at, 1);
    const char *response = at_command(modem->at, "AT+CCLK?");
    memset(&tm, 0, sizeof(struct tm));
    at_simple_scanf(response, "+CCLK: \"%d/%d/%d,%d:%d:%d%*d\"",
            &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
            &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

    /* Most modems report some starting date way in the past when they have
     * no date/time estimation. */
    if (tm.tm_year < 14) {
        errno = EINVAL;
        return 1;
    }

    /* Adjust values and perform conversion. */
    tm.tm_year += 2000 - 1900;
    tm.tm_mon -= 1;
    time_t unix_time = timegm(&tm);
    if (unix_time == -1) {
        errno = EINVAL;
        return -1;
    }

    /* All good. Return the result. */
    ts->tv_sec = unix_time;
    ts->tv_nsec = 0;
    return 0;
}

static int sim800_op_settime(struct cellular *modem, const struct timespec *ts)
{
    /* Convert time_t to broken-down UTC time. */
    struct tm tm;
    gmtime_r(&ts->tv_sec, &tm);

    /* Adjust values to match 3GPP TS 27.007. */
    tm.tm_year += 1900 - 2000;
    tm.tm_mon += 1;

    /* Set the time. */
    at_set_timeout(modem->at, 1);
    at_command_simple(modem->at, "AT+CCLK=\"%02d/%02d/%02d,%02d:%02d:%02d+00\"",
            tm.tm_year, tm.tm_mon, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);

    return 0;
}

const struct cellular_device_ops sim800_device_ops = {
    .imei = sim800_op_imei,
    .meid = NULL,
    .iccid = sim800_op_iccid,
};

const struct cellular_clock_ops sim800_clock_ops = {
    .gettime = sim800_op_gettime,
    .settime = sim800_op_settime,
};

const struct cellular_ops sim800_ops = {
    .device = &sim800_device_ops,
    .clock = &sim800_clock_ops,
};


struct cellular *cellular_sim800_alloc(struct at *at)
{
    struct cellular_sim800 *modem = malloc(sizeof(struct cellular_sim800));
    if (modem == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    modem->dev.ops = &sim800_ops;
    modem->dev.at = at;

    return (struct cellular *) modem;
}

void cellular_sim800_free(struct cellular *modem)
{
    free(modem);
}

/* vim: set ts=4 sw=4 et: */
