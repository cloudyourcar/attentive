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
#if _POSIX_TIMERS > 0
#include <time.h>
#else
#include <sys/time.h>
#endif

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
    /* The following variables are read-only after at_init(). */

    struct at_device *dev;              /**< Modem device. */

    pthread_t thread;                   /**< Response reader thread. */
    pthread_mutex_t mutex;              /**< Mutex for the variables below. */
    pthread_cond_t cond;                /**< For signalling changes in those variables. */

    size_t response_bufsize;            /**< Response accumulator size. */

    /* Parser state. Serialized on the mutex. */

    bool open : 1;                      /**< The reader thread is starting. */
    bool in_command : 1;                /**< A command is currently executing. */
    bool expect_dataprompt : 1;         /**< Expect "> " prompt for the next command. */

    int timeout;                        /**< Current command timeout, in seconds. */
    at_response_parser_t command_parser;/**< Command-specific response parser. */

    char *response;                     /**< Response accumulator. */
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

static void *at_reader(void *pdata);

struct at *at_alloc(struct at_device *dev)
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
    at->dev = dev;

    /* initialize and start reader thread */
    pthread_mutex_init(&at->mutex, NULL);
    pthread_cond_init(&at->cond, NULL);
    pthread_create(&at->thread, NULL, at_reader, (void *) at);

    return at;
}

int at_open(struct at *at)
{
    pthread_mutex_lock(&at->mutex);

    printf("at_open: opening device\n");
    int result = at->dev->ops->open(at->dev);
    if (result == 0) {
        printf("at_open: success\n");
        at->open = true;
        pthread_cond_signal(&at->cond);
    } else {
        printf("at_open: failed: %s\n", strerror(errno));
    }

    pthread_mutex_unlock(&at->mutex);
    return result;
}

int at_close(struct at *at)
{
    pthread_mutex_lock(&at->mutex);

    if (!at->open) {
        pthread_mutex_unlock(&at->mutex);
        errno = ENODEV;
        return -1;
    }

    /* Prevent subsequent commands from succeeding. */
    at->open = false;

    /* Wait for the current command to complete. */
    while (at->in_command) {
        printf("at_close: waiting for command completion\n");
        pthread_cond_wait(&at->cond, &at->mutex);
    }

    /* Close the device. */
    printf("at_close: closing device\n");
    int result = at->dev->ops->open(at->dev);
    if (result == 0) {
        printf("at_close: success\n");
        pthread_cond_signal(&at->cond);
    } else {
        printf("at_open: failed: %s\n", strerror(errno));
    }

    pthread_mutex_unlock(&at->mutex);
    return result;
}

void at_free(struct at *at)
{
    /* make sure the channel is closed */
    at_close(at);

    /* terminate reader thread */
    pthread_kill(at->thread, SIGUSR1);
    pthread_join(at->thread, NULL);
    pthread_cond_destroy(&at->cond);
    pthread_mutex_destroy(&at->mutex);

    /* free up resources */
    free(at->response);
    free(at);
}

void at_expect_dataprompt(struct at *at)
{
    pthread_mutex_lock(&at->mutex);

    at->expect_dataprompt = true;

    pthread_mutex_unlock(&at->mutex);
}

void at_set_timeout(struct at *at, int timeout)
{
    pthread_mutex_lock(&at->mutex);

    at->timeout = timeout;

    pthread_mutex_unlock(&at->mutex);
}

void at_set_command_response_parser(struct at *at, at_response_parser_t parser)
{
    pthread_mutex_lock(&at->mutex);

    at->command_parser = parser;

    pthread_mutex_unlock(&at->mutex);
}

/****************************************************************************/

static const char *_at_command(struct at *at, const void *data, size_t size)
{
    pthread_mutex_lock(&at->mutex);

    /* Bail out if the channel is closing or closed. */
    if (!at->open) {
        pthread_mutex_unlock(&at->mutex);
        errno = ENODEV;
        return NULL;
    }

    /* Prepare the response buffer. */
    at->response_length = 0;
    at->in_command = true;

    /* Send the command. */
    at->dev->ops->write(at->dev, data, size);

    /* Wait for the parser thread to collect a response. */
    if (at->timeout) {
        struct timespec ts;
#if _POSIX_TIMERS > 0
        clock_gettime(CLOCK_REALTIME, &ts);
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * 1000;
#endif
        ts.tv_sec += at->timeout;
        while (at->in_command)
            pthread_cond_timedwait(&at->cond, &at->mutex, &ts);
    } else {
        while (at->in_command)
            pthread_cond_wait(&at->cond, &at->mutex);
    }

    if (at->in_command) {
        pthread_mutex_unlock(&at->mutex);
        errno = ETIMEDOUT;
        return NULL;
    } else {
        pthread_mutex_unlock(&at->mutex);
        return at->response;
    }
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

const char *at_command_raw(struct at *at, const void *data, size_t size)
{
    printf("> [raw payload, length: %zu bytes]\n", size);

    return _at_command(at, data, size);
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
        type = at->command_parser(at->dev, line, len);
    if (!type)
        type = at->dev->ops->parse_response(at->dev, line, len);
    if (!type)
        type = generic_response_parser(line, len);
    return type;
}

/****************************************************************************/

static ssize_t at_getline(struct at_device *dev, char *buf, size_t bufsize)
{
    size_t cnt = 0;
    while (true) {
        /* Read one character. */
        char ch;
        if (dev->ops->read(dev, &ch, 1) != 1)
            return -1;

        /* Finish reading on newlines. */
        if (ch == '\r' || ch == '\n') {
            if (cnt < bufsize) {
                /* Getline succeeded, null-terminate and return. */
                buf[cnt] = '\0';
                return cnt;
            } else {
                /* Line too long. Signal error. */
                errno = ENOSPC;
                return -1;
            }
        }

        /* Append character if there's space; swallow otherwise. */
        if (cnt < bufsize-1) {
            buf[cnt++] = ch;
        }
    }
}

static void at_append_response(struct at *at, const void *buf, size_t len)
{
    if (at->response_length + (at->response_length ? 1 : 0) + len > at->response_bufsize - 1)
    {
        printf("at_append_response: insufficient buffer space\n");
        return;
    }

    /* Add a newline if there's something in the buffer. */
    if (at->response_length)
        at->response[at->response_length++] = '\n';

    /* Append received response. */
    memcpy(at->response+at->response_length, buf, len);
    at->response_length += len;
}

/**
 * AT protocol reader. Runs until at_free() is called.
 */
static void *at_reader(void *arg)
{
    struct at *at = (struct at *) arg;

    printf("at_reader: starting\n");
    while (true) {
        /* Sleep if the channel is not open. */
        pthread_mutex_lock(&at->mutex);
        while (!at->open)
            pthread_cond_wait(&at->cond, &at->mutex);
        pthread_mutex_unlock(&at->mutex);

        /* Read the next response line. */
        int len;
        char line[AT_RESPONSE_LENGTH];
        if (at->expect_dataprompt) {
            /* We're expecting "\r\n> ", read carefully. */
            if ((len = at->dev->ops->read(at->dev, line, 2)) != 2)
                break;

            /* Discard newlines. */
            if (!memcmp(line, "\r\n", 2))
                continue;

            if (memcmp(line, "> ", 2)) {
                /* Oops, this is not the dataprompt. Pretend the read didn't happen. */
                if ((len = at_getline(at->dev, line+2, sizeof(line)-2)) < 0)
                    break; // TODO: don't die on ENOSPC
            } else {
                /* Data prompt arrived. Zero-terminate and fall through. */
                line[len] = '\0';
            }
        } else {
            /* Normal operation: read a line. */
            if ((len = at_getline(at->dev, line, sizeof(line))) < 0)
                break;
        }

        /* Skip empty lines. */
        if (!len)
            continue;

        printf("< %s\n", line);

        enum at_response_type response_type = at_get_response_type(at, line, len);

        if (response_type == AT_RESPONSE_URC) {
            /* Since many misbehaved modems return URCs at random moments,
             * by default we allow URCs at any time. Override it it in your
             * modem's response parser if you want different behavior, perhaps
             * per-command. */
            at->dev->ops->handle_urc(at->dev, line, len);
            goto loop;
        }

        /* We're accessing the parser state below. Lock. */
        pthread_mutex_lock(&at->mutex);
            
        if (!at->in_command) {
            printf("at_reader: unexpected line from modem: %.*s\n", len, line);
            goto loop;
        }

        if (response_type != AT_RESPONSE_FINAL_OK) {
            /* Accumulate everything that's not a final OK. */
            at_append_response(at, line, len);
        }

        if (response_type == AT_RESPONSE_FINAL_OK || response_type == AT_RESPONSE_FINAL) {
            /* Final response. Zero-terminate the result. */
            at->response[at->response_length] = '\0';

            /* Clean up parser state. */
            at->expect_dataprompt = false;
            at->command_parser = NULL;

            /* Signal the waiting at_command(). */
            at->in_command = false;
            pthread_cond_signal(&at->cond);
            goto loop;
        }

        if (at_response_is_rawdata(response_type)) {
            /* Fixed size raw data follows. Read it and carry on reading further lines. */
            int amount = at_response_data_length(response_type);

            /* minor buffer overflow here */
            /* also, no error checking */
            len = at->dev->ops->read(at->dev, line, amount);

            at_append_response(at, line, len);
            goto loop;
        }

loop:
        pthread_mutex_unlock(&at->mutex);
    }

    printf("at_reader: finished\n");
    return NULL;
}

/* vim: set ts=4 sw=4 et: */
