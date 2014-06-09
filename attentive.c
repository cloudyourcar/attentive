/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include "attentive.h"

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define AT_COMMAND_LENGTH 80
#define AT_RESPONSE_LENGTH 256

static const char *ok_responses[] = {
    "OK",
    "> ",
    NULL
};

static const char *error_responses[] = {
    "ERROR",
    "NO CARRIER",
    "+CME ERROR:",
    "+CMS ERROR:",
    NULL,
};

static const char *urc_responses[] = {
    "RING",
    NULL,
};


/**
 * @internal
 */
struct at
{
    FILE *istream;                      /**< Modem device read stream. */
    FILE *ostream;                      /**< Modem device write stream. */
    pthread_t thread;                   /**< Response reader thread. */

    bool in_command : 1;                /**< A command is currently executing. */
    bool expect_dataprompt : 1;         /**< Expect "> " prompt for the next command. */
    int timeout;                        /**< Current command timeout, in seconds. */

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    urc_callback_t urc_callback;        /**< Modem-specific URC callback. */
    response_parser_t modem_parser;     /**< Modem-specific response parser. */
    response_parser_t command_parser;   /**< Command-specific response parser. */

    char *response;                     /**< Response accumulator. */
    size_t response_bufsize;            /**< Response accumulator size. */
    size_t response_length;             /**< Length of accumulated response. */
};


static inline bool at_response_is_rawdata(enum at_response_type response)
{
    return (response & 0xff) == _AT_RESPONSE_RAWDATA_FOLLOWS;
}

#if 0
static inline bool at_response_is_hexdata(enum at_response_type response)
{
    return (response & 0xff) == _AT_RESPONSE_HEXDATA_FOLLOWS;
}
#endif

static inline int at_response_data_length(enum at_response_type response)
{
    return response >> 8;
}

static void *at_reader_thread(void *pdata);

struct at *at_open(FILE *istream, FILE *ostream, response_parser_t modem_parser, urc_callback_t urc_callback)
{
    /* allocate instance */
    struct at *at = (struct at *) malloc(sizeof(struct at));
    if (!at) {
        errno = ENOMEM;
        return NULL;
    }
    memset(at, 0, sizeof(struct at));

    /* allocate response buffer */
    at->response = malloc(AT_RESPONSE_LENGTH);
    if (!at->response) {
        errno = ENOMEM;
        return NULL;
    }

    /* fill instance */
    at->istream = istream;
    at->ostream = ostream;
    at->urc_callback = urc_callback;
    at->modem_parser = modem_parser;

    /* start reader thread */
    pthread_mutex_init(&at->mutex, NULL);
    pthread_cond_init(&at->cond, NULL);
    pthread_create(&at->thread, NULL, at_reader_thread, (void *) at);

    return at;
}

void at_close(struct at *at)
{
    /* terminate reader thread */
    pthread_kill(at->thread, SIGTERM);
    pthread_join(at->thread, NULL);
    pthread_cond_destroy(&at->cond);
    pthread_mutex_destroy(&at->mutex);

    /* free up resources */
    free(at->response);
    free(at);
}

void at_expect_dataprompt(struct at *at)
{
    at->expect_dataprompt = true;
}

void at_set_timeout(struct at *at, int timeout)
{
    at->timeout = timeout;
}

void at_set_command_response_parser(struct at *at, response_parser_t parser)
{
    at->command_parser = parser;
}

/****************************************************************************/

static const char *_at_command(struct at *at, const void *data, size_t size)
{
    // prepare AT response handler
    at->response_length = 0;
    at->in_command = true;

    fwrite(data, 1, size, at->ostream);
    fflush(at->ostream);

    // wait for the parser thread to collect a response
    pthread_mutex_lock(&at->mutex);
    while (at->in_command)
        pthread_cond_wait(&at->cond, &at->mutex);
    pthread_mutex_unlock(&at->mutex);

    // FIXME: return NULL on errors / timeout
    return at->response;
}

const char *at_command_raw(struct at *at, const void *data, size_t size)
{
    printf("> [raw payload, length: %zu bytes]\n", size);

    return _at_command(at, data, size);
}

const char *at_command(struct at *at, const char *format, ...)
{
    /* Build command string. */
    va_list ap;
    va_start(ap, format);
    char line[AT_COMMAND_LENGTH];
    // FIXME: handle vsnprintf errors
    int len = vsnprintf(line, sizeof(line)-strlen("\r\n"), format, ap);
    va_end(ap);

    /* Append modem-style newline. */
    printf("> %s\n", line);
    memcpy(line+len, "\r\n", 2);
    len += 2;

    return _at_command(at, line, len);
}

/****************************************************************************/

bool at_prefix_in_table(const char *line, const char *table[])
{
    for (int i=0; table[i] != NULL; i++)
        if (!strncmp(line, table[i], strlen(table[i])))
            return true;
    return false;
}

static enum at_response_type generic_response_parser(const char *line, size_t len)
{
    (void) len;

    if (at_prefix_in_table(line, urc_responses))
        return AT_RESPONSE_URC;
    else if (at_prefix_in_table(line, error_responses))
        return AT_RESPONSE_FINAL;
    else if (at_prefix_in_table(line, ok_responses))
        return AT_RESPONSE_FINAL_OK;
    else
        return AT_RESPONSE_INTERMEDIATE;
}

static enum at_response_type at_get_response_type(struct at *at, const char *line, size_t len)
{
    enum at_response_type type = AT_RESPONSE_UNKNOWN;
    if (at->command_parser)
        type = at->command_parser(line, len);
    if (!type)
        type = at->modem_parser(line, len);
    if (!type)
        type = generic_response_parser(line, len);
    return type;
}

/****************************************************************************/

/**
 * AT protocol reader. Runs until an error condition happens.
 */
static void *at_reader_thread(void *arg)
{
    struct at *at = arg;

    int len;
    char line[AT_RESPONSE_LENGTH];

    printf("at_reader_task: starting\n");
    while (1) {
        if (at->expect_dataprompt) {
            /* Most modules respond with "\r\n> " when waiting for raw data. */
            len = fread(line, 1, 2, at->istream);
            if (len != 2)
                break;

            /* Discard newlines. */
            if (!memcmp(line, "\r\n", 2))
                continue;

            if (memcmp(line, "> ", 2)) {
                /* Oops, this is not the dataprompt. Pretend the fread didn't happen. */
                if (fgets(line+2, sizeof(line)-2, at->istream) == NULL)
                    break;
            } else {
                /* Data prompt arrived. Zero-terminate and fall through. */
                line[len] = '\0';
            }
        } else {
            /* Normal operation: read a line. */
            if (fgets(line, sizeof(line), at->istream) == NULL)
                break;
        }

        /* fgets() doesn't return length; work it around. */
        len = strlen(line);

        /* Remove trailing whitespace. */
        while (len > 0 && isspace(line[len-1]))
            len--;
        line[len] = '\0';

        /* Skip empty lines. */
        if (!len)
            continue;

        printf("< '%s'\n", line);

        enum at_response_type response_type = at_get_response_type(at, line, len);

        if (response_type == AT_RESPONSE_URC) {
            /* There are many misbehaved modems out there that return URCs
             * at random moments. The default is to always handle URC-like
             * lines as URCs; if you don't want this, override this behavior
             * in your modem's response parser. */
            at->urc_callback(line, len);

        } else if (at->in_command) {
            /* Accumulate everything that's not a final OK. */
            if (response_type != AT_RESPONSE_FINAL_OK) {
                if (at->response_length + (at->response_length ? 1 : 0) + len >
                        at->response_bufsize - 1)
                {
                    /* We ran out of space. Bail out. */
                    // TODO: graceful recovery
                    break;
                }

                /* Add a newline if there's something in the buffer. */
                if (at->response_length)
                    at->response[at->response_length++] = '\n';

                /* Append received response. */
                memcpy(at->response+at->response_length, line, len);
                at->response_length += len;
            }

            if (response_type == AT_RESPONSE_FINAL_OK || response_type == AT_RESPONSE_FINAL) {
                /* Zero-terminate the result. */
                at->response[at->response_length] = '\0';

                /* Final line arrived; clean up parser state. */
                at->expect_dataprompt = false;
                at->command_parser = NULL;

                /* Signal the waiting at_command(). */
                pthread_mutex_lock(&at->mutex);
                at->in_command = false;
                pthread_mutex_unlock(&at->mutex);
                pthread_cond_signal(&at->cond);

            } else if (at_response_is_rawdata(response_type)) {
                /* Fixed size raw data follows. Read it and carry on reading further lines. */
                int amount = at_response_data_length(response_type);

                if (at->response_length + (at->response_length ? 1 : 0) + amount >
                        at->response_bufsize - 1)
                {
                    /* We ran out of space. Bail out. */
                    // TODO: graceful recovery
                    break;
                }

                /* Add a newline if there's something in the buffer. */
                if (at->response_length)
                    at->response[at->response_length++] = '\n';

                /* Read a block of data and append it. */
                len = fread(at->response + at->response_length, 1, amount, at->istream);
                if (len != amount)
                    break;
                at->response_length += len;
            }

        } else {
            printf("at_reader_task: unexpected line from modem: %s\n", line);
        }
    }

    printf("at_reader_task: finished\n");

    pthread_exit(NULL);
    return NULL;
}

/* vim: set ts=4 sw=4 et: */
