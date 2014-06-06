/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef CELLULAR_H
#define CELLULAR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define CELLULAR_IMEI_LENGTH 15
#define CELLULAR_MEID_LENGTH 14
#define CELLULAR_ICCID_LENGTH 19

struct cellular_network_stats {
    int creg;
    int rssi;
    int ber;
};

struct cellular_ops {
    /** Claim exclusive access. */
    int (*claim)(struct cellular *modem);
    /** Release exclusive access. */
    int (*release)(struct cellular *modem);

    /** Read GSM modem serial number (IMEI). */
    int (*imei)(struct cellular *modem, char *buf, size_t len);
    /** Read CDMA modem serial number (MEID). */
    int (*meid)(struct cellular *modem, char *buf, size_t len);
    /** Read SIM serial number (ICCID). */
    int (*iccid)(struct cellular *modem, char *iccid);

    /** Read RTC date and time. Compatible with clock_gettime(). */
    int (*gettime)(struct cellular *modem, struct timespec *ts);
    /** Set RTC date and time. Compatible with clock_settime(). */
    int (*settime)(struct cellular *modem, const struct timespec *ts);

    struct cellular_socket_ops *socket;
    struct cellular_ftp_ops *ftp;
};

struct cellular_socket_ops {
    int (*connect)(struct cellular *modem, int connid, const char *host, uint16_t port);
    ssize_t (*send)(struct cellular *modem, int connid, const uint8_t *buffer, size_t amount, int flags);
    ssize_t (*recv)(struct cellular *modem, int connid, uint8_t *buffer, size_t length, int flags);
    int shutdown(struct cellular *modem, int connid, int how);
    int (*waitack)(struct cellular *modem, int connid, int timeout);
    int (*close)(struct cellular *modem, int connid);
};

struct cellular_ftp_ops {
    int (*open)(struct cellular *modem, const char *username, const char *password, bool passive);
    int (*get)(struct cellular *modem, const char *filename);
    int (*getpkt)(struct cellular *modem, uint8_t *buffer, size_t length);
    int (*close)(struct cellular *modem);
};

#endif

/* vim: set ts=4 sw=4 et: */
