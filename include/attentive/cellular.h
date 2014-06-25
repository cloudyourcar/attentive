/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef ATTENTIVE_CELLULAR_H
#define ATTENTIVE_CELLULAR_H

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
    int (*attach)(struct cellular *modem);
    int (*detach)(struct cellular *modem);

    const struct cellular_device_ops *device;
    const struct cellular_network_ops *network;
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


/**
 * Allocate a cellular modem instance.
 *
 * @returns Instance pointer on success, NULL and sets errno on failure.
 */
struct cellular *cellular_alloc(void);

/**
 * Attach cellular modem instance to an AT channel.
 * Performs initialization, attaches callbacks, etc.
 *
 * @param modem Cellular modem instance.
 * @param at AT channel instance.
 * @returns Zero on success, -1 and sets errno on failure.
 */
int cellular_attach(struct cellular *modem, struct at *at);

/**
 * Detach cellular modem instance.
 * @param modem Cellular modem instance.
 * @returns Zero on success, -1 and sets errno on failure.
 */
int cellular_detach(struct cellular *modem);

/**
 * Free a cellular modem instance.
 *
 * @param Modem instance allocated with cellular_alloc().
 */
void cellular_free(struct cellular *modem);


/* Modem-specific variants below. */

struct cellular *cellular_generic_alloc(void);
void cellular_generic_free(struct cellular *modem);

struct cellular *cellular_telit2_alloc(void);
void cellular_telit2_free(struct cellular *modem);

struct cellular *cellular_sim800_alloc(void);
void cellular_sim800_free(struct cellular *modem);

#endif

/* vim: set ts=4 sw=4 et: */
