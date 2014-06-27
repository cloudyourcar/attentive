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
