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
#include <unistd.h>

#include "common.h"

struct cellular_telit2 {
    struct cellular dev;
};

static enum at_response_type scan_line(const char *line, size_t len, void *arg)
{
    (void) line;
    (void) len;

    struct cellular_telit2 *priv = arg;
    (void) priv;

    return AT_RESPONSE_UNKNOWN;
}

static void handle_urc(const char *line, size_t len, void *arg)
{
    struct cellular_telit2 *priv = arg;

    printf("[telit2@%p] urc: %.*s\n", priv, (int) len, line);
}

static const struct at_callbacks telit2_callbacks = {
    .scan_line = scan_line,
    .handle_urc = handle_urc,
};

static int telit2_attach(struct cellular *modem)
{
    at_set_callbacks(modem->at, &telit2_callbacks, (void *) modem);

    at_set_timeout(modem->at, 1);
    at_command(modem->at, "AT");        /* Aid autobauding. Always a good idea. */
    at_command(modem->at, "ATE0");      /* Disable local echo. */

    /* Initialize modem. */
    static const char *const init_strings[] = {
        "AT&K0",                        /* Disable hardware flow control. */
        "AT#SELINT=2",                  /* Set Telit module compatibility level. */
        "AT+CMEE=2",                    /* Enable extended error reporting. */
        NULL
    };
    for (const char *const *command=init_strings; *command; command++)
        at_command_simple(modem->at, "%s", *command);

    return 0;
}

static int telit2_detach(struct cellular *modem)
{
    at_set_callbacks(modem->at, NULL, NULL);
    return 0;
}

static int telit2_pdp_open(struct cellular *modem, const char *apn)
{
    at_set_timeout(modem->at, 150);
    const char *response = at_command(modem->at, "AT#SGACT=1,1");

    if (response == NULL)
        return -1;

    if (!strcmp(response, "+CME ERROR: context already activated"))
        return 0;

    int ip[4];
    at_simple_scanf(response, "#SGACT: %d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]);

    return 0;
}

static int telit2_pdp_close(struct cellular *modem)
{
    at_set_timeout(modem->at, 150);
    at_command_simple(modem->at, "AT#SGACT=1,0");

    return 0;
}

int telit2_op_iccid(struct cellular *modem, char *buf, size_t len)
{
    char fmt[24];
    if (snprintf(fmt, sizeof(fmt), "#CCID: %%[0-9]%ds", (int) len) >= (int) sizeof(fmt)) {
        errno = ENOSPC;
        return -1;
    }

    at_set_timeout(modem->at, 5);
    const char *response = at_command(modem->at, "AT#CCID");
    at_simple_scanf(response, fmt, buf);
    buf[len-1] = '\0';

    return 0;
}

static int telit2_socket_connect(struct cellular *modem, int connid, const char *host, uint16_t port)
{
    /* Reset socket configuration to default. */
    at_set_timeout(modem->at, 5);
    at_command_simple(modem->at, "AT#SCFGEXT=%d,0,0,0,0,0", connid);
    at_command_simple(modem->at, "AT#SCFGEXT2=%d,0,0,0,0,0", connid);

    /* Open connection. */
    cellular_command_simple_pdp(modem, "AT#SD=%d,0,%d,%s,0,0,1", connid, port, host);

    return 0;
}

static ssize_t telit2_socket_send(struct cellular *modem, int connid, const void *buffer, size_t amount, int flags)
{
    (void) flags;

    /* Request transmission. */
    at_set_timeout(modem->at, 150);
    at_expect_dataprompt(modem->at);
    at_command_simple(modem->at, "AT#SSENDEXT=%d,%zu", connid, amount);

    /* Send raw data. */
    at_command_raw_simple(modem->at, buffer, amount);

    return amount;
}

static enum at_response_type scanner_srecv(const char *line, size_t len, void *arg)
{
    (void) len;
    (void) arg;

    int chunk;
    if (sscanf(line, "#SRECV: %*d,%d", &chunk) == 1)
        return AT_RESPONSE_RAWDATA_FOLLOWS(chunk);

    return AT_RESPONSE_UNKNOWN;
}

static ssize_t telit2_socket_recv(struct cellular *modem, int connid, void *buffer, size_t length, int flags)
{
    (void) flags;

    int cnt = 0;
    while (cnt < (int) length) {
        int chunk = (int) length - cnt;
        /* Limit read size to avoid overflowing AT response buffer. */
        if (chunk > 128)
            chunk = 128;

        /* Perform the read. */
        at_set_timeout(modem->at, 150);
        at_set_command_scanner(modem->at, scanner_srecv);
        const char *response = at_command(modem->at, "AT#SRECV=%d,%d", connid, chunk);
        if (response == NULL)
            return -1;

        /* Find the header line. */
        int bytes;
        at_simple_scanf(response, "#SRECV: %*d,%d", &bytes);

        /* Bail out if we're out of data. Message is misleading. */
        /* FIXME: We should maybe block until we receive something? */
        if (!strcmp(response, "+CME ERROR: activation failed"))
            break;

        /* Locate the payload. */
        const char *data = strchr(response, '\n');
        if (data == NULL) {
            errno = EPROTO;
            return -1;
        }
        data += 1;

        /* Copy payload to result buffer. */
        memcpy((char *)buffer + cnt, data, bytes);
        cnt += bytes;
    }

    return cnt;
}

int telit2_socket_close(struct cellular *modem, int connid)
{
    at_set_timeout(modem->at, 150);
    at_command_simple(modem->at, "AT#SH=%d", connid);

    return 0;
}


static const struct cellular_ops telit2_ops = {
    .attach = telit2_attach,
    .detach = telit2_detach,

    .pdp_open = telit2_pdp_open,
    .pdp_close = telit2_pdp_close,

    .imei = cellular_op_imei,
    .iccid = telit2_op_iccid,
    .creg = cellular_op_creg,
    .rssi = cellular_op_rssi,
    .clock_gettime = cellular_op_clock_gettime,
    .clock_settime = cellular_op_clock_settime,
    .socket_connect = telit2_socket_connect,
    .socket_send = telit2_socket_send,
    .socket_recv = telit2_socket_recv,
    .socket_close = telit2_socket_close,
};

struct cellular *cellular_telit2_alloc(void)
{
    struct cellular_telit2 *modem = malloc(sizeof(struct cellular_telit2));
    if (modem == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    memset(modem, 0, sizeof(*modem));

    modem->dev.ops = &telit2_ops;

    return (struct cellular *) modem;
}

void cellular_telit2_free(struct cellular *modem)
{
    free(modem);
}

/* vim: set ts=4 sw=4 et: */
