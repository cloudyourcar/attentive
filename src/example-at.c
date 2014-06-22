/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <assert.h>
#include <stdio.h>

#include <attentive/at.h>
#include <attentive/at-unix.h>

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
    assert(argc-1 == 1);
    const char *devpath = argv[1];

    printf("allocating parser...\n");
    struct at_parser *parser = at_parser_alloc(cbs, 256, NULL);

    printf("allocating channel...\n");
    struct at *at = at_alloc_unix(parser, devpath, B115200);
    assert(at_open(at) == 0);

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

    printf("freeing resources...\n");
    at_free(at);
    at_parser_free(parser);

    return 0;
}

/* vim: set ts=4 sw=4 et: */
