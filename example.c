/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <assert.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "attentive.h"


struct at_device_modem {
    struct at_device dev;

    const char *port;
    int fd;
    speed_t baudrate;
};

static int modem_open(struct at_device *dev)
{
    struct at_device_modem *priv = (struct at_device_modem *) dev;

    priv->fd = open(priv->port, O_RDWR);
    if (priv->fd == -1)
        return -1;

    struct termios attr;
    tcgetattr(priv->fd, &attr);
    cfsetspeed(&attr, priv->baudrate);
    tcsetattr(priv->fd, TCSANOW, &attr);

    return 0;
}

static ssize_t modem_read(struct at_device *dev, void *buf, size_t len)
{
    struct at_device_modem *priv = (struct at_device_modem *) dev;

    //printf("read %d...\n", priv->fd);
    int ret = read(priv->fd, buf, len);
    //printf("read => (%.*s) (%d)\n", (int) len, buf, ret);
    return ret;
}

static ssize_t modem_write(struct at_device *dev, const void *buf, size_t len)
{
    struct at_device_modem *priv = (struct at_device_modem *) dev;

    //printf("write %d: (%.*s)\n", priv->fd, (int) len, buf);
    return write(priv->fd, buf, len);
}

static int modem_close(struct at_device *dev)
{
    struct at_device_modem *priv = (struct at_device_modem *) dev;

    return close(priv->fd);
}

static enum at_response_type modem_parse_response(struct at_device *dev, const void *line, size_t len)
{
    (void) dev;
    (void) line;
    (void) len;

    return AT_RESPONSE_UNKNOWN;
}

static void modem_handle_urc(struct at_device *dev, const void *line, size_t len)
{
    (void) len;

    printf("[%p] URC: %.*s\n", dev, (int) len, (char *) line);
}

static const struct at_device_ops modem_ops = {
    .open = modem_open,
    .read = modem_read,
    .write = modem_write,
    .close = modem_close,

    .parse_response = modem_parse_response,
    .handle_urc = modem_handle_urc,
};

int main(int argc, char *argv[])
{
    assert(argc == 2);
    const char *port = argv[1];

    /* create AT device */
    struct at_device_modem priv = {
        .dev = { .ops = &modem_ops },
        .port = port,
        .baudrate = B115200,
    };

    /* create AT channel instance */
    printf("allocating parser...\n");
    struct at *at = at_alloc((struct at_device *) &priv);

    printf("opening port...\n");
    at_open(at);

    const char *commands[] = {
        "AT",
        "ATE0",
        "AT+CGSN",
        "AT+CCID",
        "AT+BLAH",
        "AT+CGN",
        "AT+CCID",
        NULL
    };

    printf("sending commands...\n");
    for (const char **command=commands; *command; command++) {
        const char *result = at_command(at, *command);
        printf("%s => %s\n", *command, result);
    }

    printf("closing port...\n");
    at_close(at);

    printf("destroying parser...\n");
    at_free(at);

    printf("finished\n");

    return 0;
}

/* vim: set ts=4 sw=4 et: */
