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

#include "attentive.h"



static enum at_response_type response_parser(const char *line, size_t len)
{
    (void) line;
    (void) len;

    return AT_RESPONSE_UNKNOWN;
}

static void urc_callback(const char *line, size_t len)
{
    (void) len;

    printf("URC: %s\n", line);
}

int main(int argc, char *argv[])
{
    assert(argc == 2);
    const char *port = argv[1];

    /* open serial port */
    printf("opening port...\n");
    FILE *stream = fopen(port, "r+");
    struct termios attr;
    tcgetattr(fileno(stream), &attr);
    cfsetspeed(&attr, B115200);
    tcsetattr(fileno(stream), TCSANOW, &attr);

    /* create AT channel instance */
    printf("starting parser...\n");
    struct at *at = at_open(stream, response_parser, urc_callback);

    printf("sending command...\n");
    const char *response = at_command(at, "ATE0");
    printf("response: %s\n", response);

    printf("waiting a few seconds...\n");
    sleep(3);

    at_close(at);

    return 0;
}

/* vim: set ts=4 sw=4 et: */
