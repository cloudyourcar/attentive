/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <check.h>
#include <glib.h>

#include "at_parser.h"

#define STR_LEN(s) s, strlen(s)


GQueue expected_responses = G_QUEUE_INIT;
GQueue expected_urcs = G_QUEUE_INIT;

void assert_line_expected(const void *line, size_t len, GQueue *q)
{
    const char *expected = g_queue_pop_head(q);
    ck_assert_msg(expected != NULL);
    ck_assert_str_eq(line, expected);
    ck_assert_int_eq(len, strlen(expected));
}

void handle_response(const void *line, size_t len, void *priv)
{
    (void) priv;
    assert_line_expected(line, len, &expected_responses);
}

void handle_urc(const void *line, size_t len, void *priv)
{
    (void) priv;
    assert_line_expected(line, len, &expected_urcs);
}

START_TEST(test_parser_alloc)
{
    struct at_parser_callbacks cbs = {};

    struct at_parser *parser = at_parser_alloc(&cbs, 256, NULL);
    ck_assert(parser != NULL);
    at_parser_free(parser);
}
END_TEST

START_TEST(test_parser_urc)
{
    struct at_parser_callbacks cbs = {
        .handle_response = handle_response,
        .handle_urc = handle_urc,
    };

    struct at_parser *parser = at_parser_alloc(&cbs, 256, NULL);
    ck_assert(parser != NULL);

    g_queue_push_tail(&expected_urcs, "RING");
    at_parser_feed(parser, STR_LEN("RING\r\n"));

    g_queue_push_tail(&expected_urcs, "+HERP");
    g_queue_push_tail(&expected_urcs, "+DERP");
    g_queue_push_tail(&expected_urcs, "+DERP");
    at_parser_feed(parser, STR_LEN("+HER"));
    at_parser_feed(parser, STR_LEN("P\r\n+DERP\r\n+DERP"));
    at_parser_feed(parser, STR_LEN("\r\n"));

    at_parser_free(parser);
}
END_TEST

START_TEST(test_parser_response)
{
    struct at_parser_callbacks cbs = {
        .handle_response = handle_response,
        .handle_urc = handle_urc,
    };

    struct at_parser *parser = at_parser_alloc(&cbs, 256, NULL);
    ck_assert(parser != NULL);

    g_queue_push_tail(&expected_responses, "ERROR");
    at_parser_await_response(parser, false, NULL);
    at_parser_feed(parser, STR_LEN("ERROR\r\n"));

    g_queue_push_tail(&expected_responses, "");
    at_parser_await_response(parser, false, NULL);
    at_parser_feed(parser, STR_LEN("OK\r\n"));

    at_parser_free(parser);
}
END_TEST

Suite *attentive_suite(void)
{
    Suite *s = suite_create("attentive");
    TCase *tc;
  
    tc = tcase_create("parser");
    tcase_add_test(tc, test_parser_alloc);
    tcase_add_test(tc, test_parser_urc);
    tcase_add_test(tc, test_parser_response);
    suite_add_tcase(s, tc);

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
