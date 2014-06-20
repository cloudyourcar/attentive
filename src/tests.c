/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <check.h>
#include <glib.h>

#include <attentive/parser.h>


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
    //printf("response: >>>%.*s<<< (%d)\n", (int) len, (char *) line, (int) len);
    assert_line_expected(line, len, &expected_responses);
}

void handle_urc(const void *line, size_t len, void *priv)
{
    (void) priv;
    //printf("urc: >>>%.*s<<< (%d)\n", (int) len, (char *) line, (int) len);
    assert_line_expected(line, len, &expected_urcs);
}

void expect_response(const char *line)
{
    //printf("expecting response: '%s'\n", line);
    g_queue_push_tail(&expected_responses, (gpointer) line);
}

void expect_urc(const char *line)
{
    //printf("expecting urc: '%s'\n", line);
    g_queue_push_tail(&expected_urcs, (gpointer) line);
}

void expect_prepare(void)
{
    g_queue_clear(&expected_responses);
    g_queue_clear(&expected_urcs);
}

void expect_nothing(void)
{
    ck_assert(g_queue_is_empty(&expected_responses));
    ck_assert(g_queue_is_empty(&expected_urcs));
}

START_TEST(test_parser_alloc)
{
    printf(":: test_parser_alloc\n");

    struct at_parser_callbacks cbs = {
        .handle_response = handle_response,
        .handle_urc = handle_urc,
    };
    struct at_parser *parser = at_parser_alloc(&cbs, 256, NULL);
    ck_assert(parser != NULL);
    at_parser_free(parser);
}
END_TEST

START_TEST(test_parser_response)
{
    printf(":: test_parser_response\n");

    struct at_parser_callbacks cbs = {
        .handle_response = handle_response,
        .handle_urc = handle_urc,
    };
    struct at_parser *parser = at_parser_alloc(&cbs, 256, NULL);
    ck_assert(parser != NULL);

    expect_prepare();

    expect_response("ERROR");
    at_parser_await_response(parser);
    at_parser_feed(parser, STR_LEN("ERROR\r\n"));
    expect_nothing();

    at_parser_await_response(parser);
    at_parser_feed(parser, STR_LEN("\r\n\r\n\r\n\r\n\r\n"));
    expect_nothing();
    expect_response("ERROR");
    at_parser_feed(parser, STR_LEN("ERROR\r\n"));
    expect_nothing();

    expect_response("");
    at_parser_await_response(parser);
    at_parser_feed(parser, STR_LEN("OK\r\n"));
    expect_nothing();

    expect_response("123456789");
    at_parser_await_response(parser);
    at_parser_feed(parser, STR_LEN("123456789\r\nOK\r\n"));
    expect_nothing();

    expect_response("123456789\nERROR");
    at_parser_await_response(parser);
    at_parser_feed(parser, STR_LEN("123456789\r\nERROR\r\n"));
    expect_nothing();

    at_parser_free(parser);
}
END_TEST

START_TEST(test_parser_urc)
{
    printf(":: test_parser_urc\n");

    struct at_parser_callbacks cbs = {
        .handle_response = handle_response,
        .handle_urc = handle_urc,
    };
    struct at_parser *parser = at_parser_alloc(&cbs, 256, NULL);
    ck_assert(parser != NULL);

    expect_prepare();

    expect_urc("RING");
    at_parser_feed(parser, STR_LEN("RING\r\n"));
    expect_nothing();

    expect_urc("+HERP");
    expect_urc("+DERP");
    expect_urc("+DERP");
    at_parser_feed(parser, STR_LEN("+HER"));
    at_parser_feed(parser, STR_LEN("P\r\n+DERP\r\n+DERP"));
    at_parser_feed(parser, STR_LEN("\r\n"));
    expect_nothing();

    at_parser_free(parser);
}
END_TEST

START_TEST(test_parser_mixed)
{
    printf(":: test_parser_mixed\n");

    struct at_parser_callbacks cbs = {
        .handle_response = handle_response,
        .handle_urc = handle_urc,
    };
    struct at_parser *parser = at_parser_alloc(&cbs, 256, NULL);
    ck_assert(parser != NULL);

    expect_prepare();

    expect_response("12345\n67890");
    expect_urc("RING");
    expect_urc("RING");
    expect_urc("RING");
    at_parser_await_response(parser);
    at_parser_feed(parser, STR_LEN("\r\n12345\r\nRING\r\n67890\r\nRING\r\nOK\r\n\r\nRING\r\n"));
    expect_nothing();

    at_parser_free(parser);
}
END_TEST

START_TEST(test_parser_overflow)
{
    printf(":: test_parser_overflow\n");

    struct at_parser_callbacks cbs = {
        .handle_response = handle_response,
        .handle_urc = handle_urc,
    };
    struct at_parser *parser = at_parser_alloc(&cbs, 8, NULL);
    ck_assert(parser != NULL);

    expect_prepare();

    /* this one fits... */
    expect_response("1234");
    at_parser_await_response(parser);
    at_parser_feed(parser, STR_LEN("1234\r\nOK\r\n"));
    expect_nothing();

    /* this one doesn't. */
    /* TODO: We could be better behaved when it comes to overflows. Not crashing is enough for now. */
    at_parser_await_response(parser);
    at_parser_feed(parser, STR_LEN("12345\r\nOK\r\n"));
    expect_nothing();

    at_parser_free(parser);
}
END_TEST

static enum at_response_type line_scanner(const void *line, size_t len, void *priv)
{
    (void) len;
    (void) priv;

    int bytes;
    if (sscanf(line, "+RAWDATA: %d", &bytes) == 1)
        return AT_RESPONSE_RAWDATA_FOLLOWS(bytes);

    return AT_RESPONSE_UNKNOWN;
}

START_TEST(test_parser_rawdata)
{
    printf(":: test_parser_rawdata\n");

    struct at_parser_callbacks cbs = {
        .handle_response = handle_response,
        .handle_urc = handle_urc,
        .scan_line = line_scanner,
    };
    struct at_parser *parser = at_parser_alloc(&cbs, 256, NULL);
    ck_assert(parser != NULL);

    expect_prepare();

    /* I'd love to put 0x00 here, but the entire string handling in Check croaks then. */
    expect_response("+RAWDATA: 10\nabcd\x01\xffxyzp");
    expect_urc("RING");
    expect_urc("RING");
    expect_urc("RING");
    at_parser_await_response(parser);
    at_parser_feed(parser, STR_LEN("\r\nRING\r\n+RAWDATA: 10\r\nabcd\x01\xFFxyzp\r\nRING\r\nOK\r\nRING\r\n"));
    expect_nothing();

    at_parser_free(parser);
}
END_TEST

Suite *attentive_suite(void)
{
    Suite *s = suite_create("attentive");
    TCase *tc;
  
    tc = tcase_create("parser");
    tcase_add_test(tc, test_parser_alloc);
    tcase_add_test(tc, test_parser_response);
    tcase_add_test(tc, test_parser_urc);
    tcase_add_test(tc, test_parser_mixed);
    tcase_add_test(tc, test_parser_overflow);
    tcase_add_test(tc, test_parser_rawdata);
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
