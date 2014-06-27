/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef MODEM_COMMON_H
#define MODEM_COMMON_H

#include <attentive/cellular.h>

/**
 * Request a PDP context. Opens one if isn't already active.
 *
 * @returns Zero on success, -1 and sets errno on failure
 */
int cellular_pdp_request(struct cellular *modem);

/**
 * Signal network connection success.
 */
void cellular_pdp_success(struct cellular *modem);

/**
 * Signal network connection failure.
 */
void cellular_pdp_failure(struct cellular *modem);

/**
 * Perform a network command, requesting a PDP context and signalling success
 * or failure to the PDP machinery. Returns -1 on failure.
 */
#define cellular_command_simple_pdp(modem, command...)                      \
    do {                                                                    \
        /* Attempt to establish a PDP context. */                           \
        if (cellular_pdp_request(modem) != 0)                               \
            return -1;                                                      \
        /* Send the command */                                              \
        const char *netresponse = at_command(modem->at, command);           \
        if (netresponse == NULL || strcmp(netresponse, "")) {               \
            cellular_pdp_failure(modem);                                    \
            return -1;                                                      \
        } else {                                                            \
            cellular_pdp_success(modem);                                    \
        }                                                                   \
    } while (0)

/*
 * 3GPP TS 27.007 compatible operations.
 */

int cellular_op_imei(struct cellular *modem, char *buf, size_t len);
int cellular_op_iccid(struct cellular *modem, char *buf, size_t len);
int cellular_op_creg(struct cellular *modem);
int cellular_op_rssi(struct cellular *modem);
int cellular_op_clock_gettime(struct cellular *modem, struct timespec *ts);
int cellular_op_clock_settime(struct cellular *modem, const struct timespec *ts);

#endif

/* vim: set ts=4 sw=4 et: */
