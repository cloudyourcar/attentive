/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <attentive/cellular.h>

int cellular_attach(struct cellular *modem, struct at *at)
{
    modem->at = at;
    if (modem->ops->attach)
        return modem->ops->attach(modem);
    else
        return 0;
}

int cellular_detach(struct cellular *modem)
{
    if (modem->ops->detach)
        return modem->ops->detach(modem);
    else
        return 0;
}

/* vim: set ts=4 sw=4 et: */
