/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <attentive/cellular.h>

#include "modem/common.h"


int cellular_attach(struct cellular *modem, struct at *at, const char *apn)
{
    /* Do nothing if we're already attached. */
    if (modem->at)
        return 0;

    modem->at = at;
    modem->apn = apn;

    /* Reset PDP failure counters. */
    cellular_pdp_success(modem);

    return modem->ops->attach ? modem->ops->attach(modem) : 0;
}

int cellular_detach(struct cellular *modem)
{
    /* Do nothing if we're not attached. */
    if (!modem->at)
        return 0;

    int result = modem->ops->detach? modem->ops->detach(modem) : 0;
    modem->at = NULL;
    return result;
}

/* vim: set ts=4 sw=4 et: */
