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

#include "common.h"


struct cellular_generic {
    struct cellular dev;
};

static const struct cellular_ops generic_ops = {
    .imei = cellular_op_imei,
    .iccid = cellular_op_iccid,
    .creg = cellular_op_creg,
    .rssi = cellular_op_rssi,
    .clock_gettime = cellular_op_clock_gettime,
    .clock_settime = cellular_op_clock_settime,
};


struct cellular *cellular_generic_alloc(void)
{
    struct cellular_generic *modem = malloc(sizeof(struct cellular_generic));
    if (modem == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    modem->dev.ops = &generic_ops;

    return (struct cellular *) modem;
}

void cellular_generic_free(struct cellular *modem)
{
    free(modem);
}

/* vim: set ts=4 sw=4 et: */
