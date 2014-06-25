#ifndef MODEM_GENERIC_H
#define MODEM_GENERIC_H

#include <attentive/cellular.h>

/*
 * Export the generic ops so that other modems can use them.
 */

int generic_op_imei(struct cellular *modem, char *buf, size_t len);
int generic_op_iccid(struct cellular *modem, char *buf, size_t len);
int generic_op_creg(struct cellular *modem);
int generic_op_rssi(struct cellular *modem);
int generic_op_gettime(struct cellular *modem, struct timespec *ts);
int generic_op_settime(struct cellular *modem, const struct timespec *ts);

#endif

/* vim: set ts=4 sw=4 et: */
