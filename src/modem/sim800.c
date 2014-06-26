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

#include "generic.h"

/*
 * SIM800 probably holds the highly esteemed position of the world's worst
 * behaving GSM modem, ever. The following quirks have been spotted so far:
 * - response continues after OK (AT+CIPSTATUS)
 * - response without a final OK (AT+CIFSR)
 * - freeform URCs coming at random moments like "DST: 1" (AT+CLTS=1)
 * - text-only URCs like "NORMAL POWER DOWN"
 * - suffix-based URCs like "1, CONNECT OK" (AT+CIPSTART)
 * - bizarre OK responses like "SHUT OK" (AT+CIPSHUT)
 * - responses without a final OK (sic!) (AT+CIFSR)
 * - no response at all (AT&K0)
 * We work it all around, but it makes the code unnecessarily complex.
 */

enum sim800_socket_status {
    SIM800_SOCKET_STATUS_ERROR = -1,
    SIM800_SOCKET_STATUS_UNKNOWN = 0,
    SIM800_SOCKET_STATUS_CONNECTED = 1,
};

#define SIM800_NSOCKETS                 6
#define SIM800_CONNECT_TIMEOUT          60
#define SIM800_CIPCFG_RETRIES           10

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
    enum sim800_socket_status socket_status[SIM800_NSOCKETS];
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


static int sim800_attach(struct cellular *modem)
{
    at_set_callbacks(modem->at, &sim800_callbacks, (void *) modem);

    at_set_timeout(modem->at, 1);
    at_command(modem->at, "AT");        /* Aid autobauding. Always a good idea. */
    at_command(modem->at, "ATE0");      /* Disable local echo. */

    /* Initialize modem. */
    static const char *init_strings[] = {
        "AT+IFC=0,0",                   /* Disable hardware flow control. */
        "AT+CMEE=2",                    /* Enable extended error reporting. */
        NULL
    };
    for (const char **command=init_strings; *command; command++)
        at_command_simple(modem->at, "%s", *command);

    return 0;
}

static int sim800_detach(struct cellular *modem)
{
    at_set_callbacks(modem->at, NULL, NULL);
    return 0;
}


/**
 * SIM800 IP configuration commands fail if the IP application is running,
 * even though the configuration settings are already right. The following
 * monkey dance is therefore needed.
 */
static int sim800_config(struct cellular *modem, const char *option, const char *value, int attempts)
{
    at_set_timeout(modem->at, 10);

	for (int i=0; i<attempts; i++) {
        /* Blindly try to set the configuration option. */
		at_command(modem->at, "AT+%s=%s", option, value);

        /* Query the setting status. */
		const char *response = at_command(modem->at, "AT+%s?", option);
        /* Bail out on timeouts. */
        if (response == NULL)
            return -1;

        /* Check if the setting has the correct value. */
        char expected[16];
        if (snprintf(expected, sizeof(expected), "+%s: %s", option, value) >= (int) sizeof(expected)) {
            errno = ENOBUFS;
            return -1;
        }
		if (!strcmp(response, expected))
			return 0;

        sleep(1);
	}

    errno = ETIMEDOUT;
    return -1;
}

static enum at_response_type scanner_cipstatus(const void *line, size_t len, void *arg)
{
    (void) len;
    (void) arg;

    /* There are response lines after OK. Keep reading. */
    if (!strcmp(line, "OK"))
        return AT_RESPONSE_INTERMEDIATE;
    /* Collect the entire post-OK response until the last C: line. */
    if (!strncmp(line, "C: 5", 4))
        return AT_RESPONSE_FINAL;
    return AT_RESPONSE_UNKNOWN;
}

/**
 * Retrieve AT+CIPSTATUS state.
 *
 * @returns 1 if context is open, 0 if context is closed, -1 on errors.
 */
static int sim800_ipstatus(struct cellular *modem)
{
    at_set_timeout(modem->at, 10);
    at_set_command_scanner(modem->at, scanner_cipstatus);
    const char *response = at_command(modem->at, "AT+CIPSTATUS");

    const char *state = strstr(response, "STATE: ");
    if (!state) {
        errno = EPROTO;
        return -1;
    }
    state += strlen("STATE: ");
    if (!strncmp(state, "IP STATUS", strlen("IP STATUS")))
        return 1;
    if (!strncmp(state, "IP PROCESSING", strlen("IP PROCESSING")))
        return 1;

    return 0;
}

static enum at_response_type scanner_cifsr(const void *line, size_t len, void *arg)
{
    (void) len;
    (void) arg;

    /* Accept an IP address as an OK response. */
    int ip[4];
    if (sscanf(line, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]) == 4)
        return AT_RESPONSE_FINAL_OK;
    return AT_RESPONSE_UNKNOWN;
}

static int sim800_pdp_open(struct cellular *modem, const char *apn)
{
    /* Do nothing if the context is already open. */
    if (sim800_ipstatus(modem) == 1)
        return 0;

    /* Switch to multiple connections mode; it's less buggy. */
    if (sim800_config(modem, "CIPMUX", "1", SIM800_CIPCFG_RETRIES) != 0)
        return -1;
    /* Receive data manually. */
    if (sim800_config(modem, "CIPRXGET", "1", SIM800_CIPCFG_RETRIES) != 0)
        return -1;
    /* Enable quick send mode. */
    if (sim800_config(modem, "CIPQSEND", "1", SIM800_CIPCFG_RETRIES) != 0)
        return -1;

    at_set_timeout(modem->at, 150);

    /* Configure context for FTP/HTTP applications. */
    at_command_simple(modem->at, "AT+SAPBR=3,1,APN,\"%s\"", apn);

    /* Commands below don't check the response. This is intentional; instead
     * of trying to stay in sync with the GPRS state machine we blindly issue
     * the command sequence needed to transition through all the states and
     * reach IP STATUS. See SIM800 Series_TCPIP_Application Note_V1.01.pdf for
     * the GPRS states documentation. */

    /* Configure context for TCP/IP applications. */
    at_command(modem->at, "AT+CSTT=\"%s\"", apn);
    /* Establish context. */
    at_command(modem->at, "AT+CIICR");
    /* Read local IP address. Switches modem to IP STATUS state. */
    at_set_command_scanner(modem->at, scanner_cifsr);
    at_command(modem->at, "AT+CIFSR");

    return sim800_ipstatus(modem);
}


static enum at_response_type scanner_cipshut(const void *line, size_t len, void *arg)
{
    (void) arg;
    if (!strncmp(line, "SHUT OK", len))
        return AT_RESPONSE_FINAL_OK;
    return AT_RESPONSE_UNKNOWN;
}

static int sim800_pdp_close(struct cellular *modem)
{
    at_set_command_scanner(modem->at, scanner_cipshut);
    at_command_simple(modem->at, "AT+CIPSHUT");

    return 0;
}


static int sim800_socket_connect(struct cellular *modem, int connid, const char *host, uint16_t port)
{
    struct cellular_sim800 *priv = (struct cellular_sim800 *) modem;

    /* TODO: Missing PDP support. */

    /* Send connection request. */
    at_set_timeout(modem->at, 150);
    priv->socket_status[connid] = SIM800_SOCKET_STATUS_UNKNOWN;
    at_command_simple(modem->at, "AT+CIPSTART=%d,TCP,\"%s\",%d", connid, host, port);

    /* Wait for socket status URC. */
    for (int i=0; i<SIM800_CONNECT_TIMEOUT; i++) {
        if (priv->socket_status[connid] == SIM800_SOCKET_STATUS_CONNECTED) {
            return 0;
        } else if (priv->socket_status[connid] == SIM800_SOCKET_STATUS_ERROR) {
            errno = ECONNABORTED;
            return -1;
        }
        sleep(1);
    }

    errno = ETIMEDOUT;
    return -1;
}

static enum at_response_type scanner_cipsend(const void *line, size_t len, void *arg)
{
    (void) arg;

    int connid, amount;
    if (sscanf(line, "DATA ACCEPT:%d,%d", &connid, &amount) == 2)
        return AT_RESPONSE_FINAL_OK;
    if (!strncmp(line, "SEND OK", len))
        return AT_RESPONSE_FINAL_OK;
    if (!strncmp(line, "SEND FAIL", len))
        return AT_RESPONSE_FINAL;
    return AT_RESPONSE_UNKNOWN;
}

static ssize_t sim800_socket_send(struct cellular *modem, int connid, const void *buffer, size_t amount, int flags)
{
    (void) flags;

    /* Request transmission. */
    at_set_timeout(modem->at, 150);
    at_expect_dataprompt(modem->at);
    at_command_simple(modem->at, "AT+CIPSEND=%d,%zu", connid, amount);

    /* Send raw data. */
    at_set_command_scanner(modem->at, scanner_cipsend);
    at_command_raw_simple(modem->at, buffer, amount);

    return amount;
}

static enum at_response_type scanner_cipclose(const void *line, size_t len, void *arg)
{
    (void) len;
    (void) arg;

    int connid;
    char last;
    if (sscanf(line, "%d, CLOSE O%c", &connid, &last) == 2 && last == 'K')
        return AT_RESPONSE_FINAL_OK;
    return AT_RESPONSE_UNKNOWN;
}

int sim800_socket_close(struct cellular *modem, int connid)
{
    at_set_timeout(modem->at, 150);
    at_set_command_scanner(modem->at, scanner_cipclose);
    at_command_simple(modem->at, "AT+CIPCLOSE=%d", connid);

    return 0;
}

static const struct cellular_ops sim800_ops = {
    .attach = sim800_attach,
    .detach = sim800_detach,

    .pdp_open = sim800_pdp_open,
    .pdp_close = sim800_pdp_close,

    .imei = generic_op_imei,
    .iccid = generic_op_iccid,
    .creg = generic_op_creg,
    .rssi = generic_op_rssi,
    .clock_gettime = generic_op_clock_gettime,
    .clock_settime = generic_op_clock_settime,
    .socket_connect = sim800_socket_connect,
    .socket_send = sim800_socket_send,
    .socket_close = sim800_socket_close,
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
