/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <check.h>

#include "attentive.h"

START_TEST(test_attentive_dummy)
{
    ck_assert(true);
}
END_TEST

Suite *attentive_suite(void)
{
    Suite *s = suite_create ("attentive");
  
    TCase *tc_dummy = tcase_create("attentive_dummy");
    tcase_add_test(tc_dummy, test_attentive_dummy);
    suite_add_tcase(s, tc_dummy);

    return s;
}

int main()
{
    int number_failed;
    Suite *s = attentive_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set ts=4 sw=4 et: */
