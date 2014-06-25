/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <attentive/cellular.h>

#include <stdio.h>
#include <string.h>

#include "generic.h"


struct cellular_sim800 {
    struct cellular dev;
};


static const struct cellular_device_ops sim800_device_ops = {
    .imei = generic_op_imei,
    .iccid = generic_op_iccid,
};

static const struct cellular_network_ops sim800_network_ops = {
    .creg = generic_op_creg,
    .rssi = generic_op_rssi,
};

static const struct cellular_clock_ops sim800_clock_ops = {
    .gettime = generic_op_gettime,
    .settime = generic_op_settime,
};

static const struct cellular_ops sim800_ops = {
    .device = &sim800_device_ops,
    .network = &sim800_network_ops,
    .clock = &sim800_clock_ops,
};

struct cellular *cellular_sim800_alloc(void)
{
    struct cellular_sim800 *modem = malloc(sizeof(struct cellular_sim800));
    if (modem == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    memset(modem, 0, sizeof(*modem));

    modem->dev.ops = &sim800_ops;

    return (struct cellular *) modem;
}

void cellular_sim800_free(struct cellular *modem)
{
    free(modem);
}

/* vim: set ts=4 sw=4 et: */
