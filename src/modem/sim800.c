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

static const char *sim800_urc_responses[] = {
    "+CIPRXGET: 1,",    /* incoming socket data notification */
    "+FTPGET: 1,",      /* FTP state change notification */
    "+PDP: DEACT",      /* PDP disconnected */
    "+SAPBR 1: DEACT",  /* PDP disconnected (for SAPBR apps) */
    NULL
};

struct cellular_sim800 {
    struct cellular dev;

    int ftpget1_status;
};

static enum at_response_type scan_line(const void *line, size_t len, void *arg)
{
    (void) len; /* FIXME: we could do better */
    (void) arg;

    if (at_prefix_in_table(line, sim800_urc_responses))
        return AT_RESPONSE_URC;

    return AT_RESPONSE_UNKNOWN;
}

static void handle_urc(const void *line, size_t len, void *arg)
{
    (void) len;
    struct cellular_sim800 *modem = arg;

    printf("[SIM800@%p] URC: %.*s\n", arg, (int) len, line);

    if (sscanf(line, "+FTPGET: 1,%d", &modem->ftpget1_status) == 1)
        return;
}

static const struct at_callbacks sim800_callbacks = {
    .scan_line = scan_line,
    .handle_urc = handle_urc,
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

static int sim800_attach(struct cellular *modem)
{
    at_set_callbacks(modem->at, &sim800_callbacks, (void *) modem);
    return 0;
}

static int sim800_detach(struct cellular *modem)
{
    at_set_callbacks(modem->at, NULL, NULL);
    return 0;
}

static const struct cellular_ops sim800_ops = {
    .attach = sim800_attach,
    .detach = sim800_detach,

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
