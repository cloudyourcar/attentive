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

#include <attentive/at.h>


#define CELLULAR_IMEI_LENGTH 15
#define CELLULAR_MEID_LENGTH 14
#define CELLULAR_ICCID_LENGTH 19


enum {
    CREG_NOT_REGISTERED = 0,
    CREG_REGISTERED_HOME = 1,
    CREG_SEARCHING = 2,
    CREG_REGISTRATION_DENIED = 3,
    CREG_UNKNOWN = 4,
    CREG_REGISTERED_ROAMING = 5,
};

struct cellular {
    const struct cellular_ops *ops;
    struct at *at;
};

struct cellular_ops {
    /** Claim exclusive access. */
    int (*claim)(struct cellular *modem);
    /** Release exclusive access. */
    int (*release)(struct cellular *modem);

    const struct cellular_device_ops *device;
    const struct cellular_clock_ops *clock;
    const struct cellular_socket_ops *socket;
    const struct cellular_ftp_ops *ftp;
};

struct cellular_device_ops {
    /** Read GSM modem serial number (IMEI). */
    int (*imei)(struct cellular *modem, char *buf, size_t len);
    /** Read CDMA modem serial number (MEID). */
    int (*meid)(struct cellular *modem, char *buf, size_t len);
    /** Read SIM serial number (ICCID). */
    int (*iccid)(struct cellular *modem, char *iccid, size_t len);
};

struct cellular_network_ops {
    /** Get network registration status. */
    int (*creg)(struct cellular *modem);
    /** Get signal strength. */
    int (*rssi)(struct cellular *modem);
};

struct cellular_clock_ops {
    /** Read RTC date and time. Compatible with clock_gettime(). */
    int (*gettime)(struct cellular *modem, struct timespec *ts);
    /** Set RTC date and time. Compatible with clock_settime(). */
    int (*settime)(struct cellular *modem, const struct timespec *ts);
};

struct cellular_socket_ops {
    int (*connect)(struct cellular *modem, int connid, const char *host, uint16_t port);
    ssize_t (*send)(struct cellular *modem, int connid, const void *buffer, size_t amount, int flags);
    ssize_t (*recv)(struct cellular *modem, int connid, void *buffer, size_t length, int flags);
    int (*shutdown)(struct cellular *modem, int connid, int how);
    int (*waitack)(struct cellular *modem, int connid, int timeout);
    int (*close)(struct cellular *modem, int connid);
};

struct cellular_ftp_ops {
    int (*open)(struct cellular *modem, const char *username, const char *password, bool passive);
    int (*get)(struct cellular *modem, const char *filename);
    int (*getpkt)(struct cellular *modem, void *buffer, size_t length);
    int (*close)(struct cellular *modem);
};

struct cellular *cellular_telit2_alloc(struct at *at);
void cellular_telit2_free(struct cellular *modem);

struct cellular *cellular_sim800_alloc(struct at *at);
void cellular_sim800_free(struct cellular *modem);

#endif

/* vim: set ts=4 sw=4 et: */
