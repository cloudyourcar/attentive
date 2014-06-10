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

/* Standard UNIX serial port. */

struct at_port_unix_serial {
    struct at_port dev;

    const char *devpath;
    int fd;
    speed_t baudrate;
};

static int unix_serial_open(struct at_port *dev)
{
    struct at_port_unix_serial *priv = (struct at_port_unix_serial *) dev;

    priv->fd = open(priv->devpath, O_RDWR);
    if (priv->fd == -1)
        return -1;

    struct termios attr;
    tcgetattr(priv->fd, &attr);
    cfsetspeed(&attr, priv->baudrate);
    tcsetattr(priv->fd, TCSANOW, &attr);

    return 0;
}

static ssize_t unix_serial_read(struct at_port *dev, void *buf, size_t len)
{
    struct at_port_unix_serial *priv = (struct at_port_unix_serial *) dev;

    return read(priv->fd, buf, len);
}

static ssize_t unix_serial_write(struct at_port *dev, const void *buf, size_t len)
{
    struct at_port_unix_serial *priv = (struct at_port_unix_serial *) dev;

    return write(priv->fd, buf, len);
}

static int unix_serial_close(struct at_port *dev)
{
    struct at_port_unix_serial *priv = (struct at_port_unix_serial *) dev;

    return close(priv->fd);
}

static const struct at_port_operations unix_serial_ops = {
    .open  = unix_serial_open,
    .read  = unix_serial_read,
    .write = unix_serial_write,
    .close = unix_serial_close,
};

/* Generic (no-op) modem device. */

struct at_device_generic {
    struct at_device dev;
};

static enum at_response_type generic_modem_parse_response(struct at_device *dev, const void *line, size_t len)
{
    (void) dev;
    (void) line;
    (void) len;

    return AT_RESPONSE_UNKNOWN;
}

static void generic_modem_handle_urc(struct at_device *dev, const void *line, size_t len)
{
    (void) len;

    printf("[%p] URC: %.*s\n", dev, (int) len, (char *) line);
}

static const struct at_device_operations generic_modem_ops = {
    .parse_response = generic_modem_parse_response,
    .handle_urc = generic_modem_handle_urc,
};

int main(int argc, char *argv[])
{
    assert(argc == 2);
    const char *devpath = argv[1];

    struct at_port_unix_serial port = {
        .dev = { &unix_serial_ops },
        .baudrate = B115200,
        .devpath = devpath,
    };

    struct at_device_generic device = {
        .dev = { &generic_modem_ops, },
    };

    /* create AT channel instance */
    printf("allocating parser...\n");
    struct at *at = at_alloc((struct at_port *) &port, (struct at_device *) &device);

    printf("opening port...\n");
    at_open(at);

    const char *commands[] = {
        "AT",
        "ATE0",
        "AT+CGSN",
        "AT+CCID",
        "AT+CGN",
        "AT+CMEE=0",
        "AT+BLAH",
        "AT+CMEE=2",
        "AT+BLAH",
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
