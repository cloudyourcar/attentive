/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <assert.h>
#include <stdio.h>

#include <attentive/parser.h>
#include <attentive/at.h>
#include <attentive/at-unix.h>
#include <attentive/cellular.h>

int main(int argc, char *argv[])
{
    assert(argc-1 == 1);
    const char *devpath = argv[1];

    struct at_parser *parser = at_parser_alloc(cbs, 256, NULL);
    struct at *at = at_alloc_unix(parser, devpath, B115200);
    struct cellular *modem = cellular_sim800_alloc(at);

    at_open(at);

    at_command(at, "AT");
    at_command(at, "ATE0");
    at_command(at, "AT+CGMR");
    at_command(at, "AT+CGSN");

    struct timespec ts;
    modem->ops->clock->gettime(modem, &ts);

    char imei[CELLULAR_IMEI_LENGTH+1];
    modem->ops->device->imei(modem, imei, sizeof(imei));
    printf("imei: %s\n", imei);

    at_close(at);

    cellular_sim800_free(modem);
    at_free(at);
    at_parser_free(parser);

    return 0;
}

/* vim: set ts=4 sw=4 et: */
