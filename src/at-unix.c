/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <attentive/at.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#if _POSIX_TIMERS > 0
#include <time.h>
#else
#include <sys/time.h>
#endif

// Remove once you refactor this out.
#define AT_COMMAND_LENGTH 80


#ifdef PARITY_ERR_SIMULATION
volatile bool errflag = false;
#endif

struct at_unix {
    struct at at;

    const char *devpath;    /**< Serial port device path. */
    speed_t baudrate;       /**< Serial port baudate. */
    enum parity_t parity;		/**< Serial port parity. */
    volatile int errParityCtr;
    int timeout;            /**< Command timeout in seconds. */
    const char *response;

    pthread_t thread;       /**< Reader thread. */
    pthread_mutex_t mutex;  /**< Protects variables below and the parser. */
    pthread_cond_t cond;    /**< For signalling open/busy release. */

    int fd;                 /**< Serial port file descriptor. */
    bool running : 1;       /**< Reader thread should be running. */
    bool open : 1;          /**< FD is valid. Set/cleared by open()/close(). */
    bool busy : 1;          /**< FD is in use. Set/cleared by reader thread. */
    bool waiting : 1;       /**< Waiting for response callback to arrive. */
};

void *at_reader_thread(void *arg);

static void handle_sigusr1(int signal)
{
    (void)signal;
}

static void handle_response(const char *buf, size_t len, void *arg)
{
    struct at_unix *priv = (struct at_unix *) arg;

    /* The mutex is held by the reader thread; don't reacquire. */
    priv->response = buf;
    (void) len;
    priv->waiting = false;
    pthread_cond_signal(&priv->cond);
}

static void handle_urc(const char *buf, size_t len, void *arg)
{
    struct at *at = (struct at *) arg;

    /* Forward to caller's URC callback, if any. */
    if (at->cbs->handle_urc)
        at->cbs->handle_urc(buf, len, at->arg);
}

enum at_response_type scan_line(const char *line, size_t len, void *arg)
{
    struct at *at = (struct at *) arg;

    enum at_response_type type = AT_RESPONSE_UNKNOWN;
    if (at->command_scanner)
        type = at->command_scanner(line, len, at->arg);
    if (!type && at->cbs && at->cbs->scan_line)
        type = at->cbs->scan_line(line, len, at->arg);
    return type;
}

static const struct at_parser_callbacks parser_callbacks = {
    .handle_response = handle_response,
    .handle_urc = handle_urc,
    .scan_line = scan_line,
};

struct at *at_alloc_unix(const char *devpath, speed_t baudrate)
{
    /* allocate instance */
    struct at_unix *priv = malloc(sizeof(struct at_unix));
    if (!priv) {
        errno = ENOMEM;
        return NULL;
    }
    memset(priv, 0, sizeof(struct at_unix));

    /* allocate underlying parser */
    priv->at.parser = at_parser_alloc(&parser_callbacks, 256, (void *) priv);
    if (!priv->at.parser) {
        free(priv);
        return NULL;
    }

    /* copy over device parameters */
    priv->devpath = devpath;
    priv->baudrate = baudrate;

    /* install empty SIGUSR1 handler */
    struct sigaction sa = {
        .sa_handler = handle_sigusr1,
    };

    sigaction(SIGUSR1, &sa, NULL);

    /* initialize and start reader thread */
    priv->running = true;
    pthread_mutex_init(&priv->mutex, NULL);
    pthread_cond_init(&priv->cond, NULL);
    pthread_create(&priv->thread, NULL, at_reader_thread, (void *) priv);

    return (struct at *) priv;
}


void at_set_parity(struct at *at, enum parity_t Parity)
{
    struct at_unix *priv = (struct at_unix *) at;
    struct termios attr;
    tcgetattr(priv->fd, &attr);

    switch(Parity)
    {
        case PARITY_ODD:
            attr.c_iflag |= PARMRK ;
            attr.c_iflag |= INPCK ;
            attr.c_cflag |= PARENB ;	 //Enable parity
            attr.c_cflag |= PARODD ;     //Parity is odd
            attr.c_iflag &=~IGNPAR ;
            break;

        case PARITY_EVEN:
        	attr.c_iflag |= PARMRK ;
            attr.c_iflag |= INPCK ;
            attr.c_cflag |= PARENB ;     //Enable parity
            attr.c_cflag &= ~PARODD ;    //Parity is even
            attr.c_iflag &=~IGNPAR ;
            break;

        case PARITY_NONE:
        default:
        	attr.c_iflag &= ~PARMRK ;
        	attr.c_iflag &= ~INPCK ;
            attr.c_cflag &= ~PARODD ;
            attr.c_cflag &= ~PARENB ;    //Disable parity
            attr.c_iflag |= IGNPAR ;     //Ignore parity errore
            break;
    }

   int  i = tcsetattr(priv->fd, TCSANOW, &attr);
   printf("GSM Module %i",i);
}

enum parity_t at_get_parity(struct at *at)
{
    struct at_unix *priv = (struct at_unix *) at;
    struct termios attr;
    enum parity_t parity;

    tcgetattr(priv->fd, &attr);

    if(attr.c_cflag & PARENB )
    {
        if(attr.c_cflag & PARODD )
        {
        	parity = PARITY_ODD;
        }
        else
        {
            parity = PARITY_EVEN;
        }
    }
    else
    {
        parity = PARITY_NONE;
    }
    return parity;
}



int at_open(struct at *at)
{
    struct at_unix *priv = (struct at_unix *) at;
    struct termios attr;

    pthread_mutex_lock(&priv->mutex);
    if (priv->open) {
        pthread_mutex_unlock(&priv->mutex);
        return 0;
    }

    priv->fd = open(priv->devpath, O_RDWR);
    if (priv->fd == -1) {
        pthread_mutex_unlock(&priv->mutex);
        return -1;
    }
    if (priv->baudrate) {
        struct termios attr;
        tcgetattr(priv->fd, &attr);
        cfsetspeed(&attr, priv->baudrate);
        tcsetattr(priv->fd, TCSANOW, &attr);
    }

    priv->open = true;
    pthread_cond_signal(&priv->cond);
    pthread_mutex_unlock(&priv->mutex);

    return 0;
}

int at_close(struct at *at)
{
    struct at_unix *priv = (struct at_unix *) at;

    pthread_mutex_lock(&priv->mutex);
    if (!priv->open) {
        pthread_mutex_unlock(&priv->mutex);
        return 0;
    }

    /* Mark the port descriptor as invalid. */
    priv->open = false;

    /* Interrupt read() in the reader thread. */
    pthread_kill(priv->thread, SIGUSR1);

    /* Wait for the read operation to complete. */
    while (priv->busy)
        pthread_cond_wait(&priv->cond, &priv->mutex);

    /* Close the file descriptor. */
    close(priv->fd);
    priv->fd = -1;

    pthread_mutex_unlock(&priv->mutex);
    return 0;
}

void at_free(struct at *at)
{
    struct at_unix *priv = (struct at_unix *) at;

    /* make sure the channel is closed */
    at_close(at);

    /* ask the reader thread to terminate */
    pthread_mutex_lock(&priv->mutex);
    priv->running = false;
    pthread_mutex_unlock(&priv->mutex);

    /* wait for the reader thread to terminate */
    pthread_kill(priv->thread, SIGUSR1);
    pthread_join(priv->thread, NULL);
    pthread_cond_destroy(&priv->cond);
    pthread_mutex_destroy(&priv->mutex);

    /* free up resources */
    free(priv->at.parser);
    free(priv);
}

void at_set_callbacks(struct at *at, const struct at_callbacks *cbs, void *arg)
{
    at->cbs = cbs;
    at->arg = arg;
}

void at_set_command_scanner(struct at *at, at_line_scanner_t scanner)
{
    at->command_scanner = scanner;
}

void at_set_timeout(struct at *at, int timeout)
{
    struct at_unix *priv = (struct at_unix *) at;

    priv->timeout = timeout;
}

bool at_handle_transfer_errors(struct at *at)
{
    struct at_unix *priv = (struct at_unix *) at;
    pthread_mutex_lock(&priv->mutex);
    bool retval = false;
    if((priv->errParityCtr) > 0 )
    {
    	priv->errParityCtr--;
    	retval = true;
    }
    pthread_mutex_unlock(&priv->mutex);
    return retval;
}

void at_expect_dataprompt(struct at *at)
{
    at_parser_expect_dataprompt(at->parser);
}

static const char *_at_command(struct at_unix *priv, const void *data, size_t size)
{
    pthread_mutex_lock(&priv->mutex);

    /* Bail out if the channel is closing or closed. */
    if (!priv->open) {
        pthread_mutex_unlock(&priv->mutex);
        errno = ENODEV;
        return NULL;
    }

    /* Prepare parser. */
    at_parser_await_response(priv->at.parser);

    /* Send the command. */
    // FIXME: handle interrupts, short writes, errors, etc.
#ifdef PARITY_ERR_SIMULATION
    if(errflag)
    {
         errflag = false;
         struct termios attr;
         int rc = tcgetattr(priv->fd, &attr);
         int speed = cfgetispeed(&attr);
         attr.c_cflag |=  CSTOPB;
         if(speed == 57600)
         {
             rc = cfsetispeed(&attr,B38400);
         }
         else
         {
             rc = cfsetispeed(&attr,B57600);
         }
         int rc2 = tcsetattr(priv->fd, TCSANOW, &attr);
         printf("speed %i rc %i %i\n", speed,rc,rc2);
    }
#endif
    write(priv->fd, data, size);

    /* Wait for the parser thread to collect a response. */
    priv->waiting = true;
    if (priv->timeout) {
        struct timespec ts;
#if _POSIX_TIMERS > 0
        clock_gettime(CLOCK_REALTIME, &ts);
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * 1000;
#endif
        ts.tv_sec += priv->timeout;

        while (priv->open && priv->waiting)
            if (pthread_cond_timedwait(&priv->cond, &priv->mutex, &ts) == ETIMEDOUT)
                break;
    } else {
        while (priv->open && priv->waiting)
            pthread_cond_wait(&priv->cond, &priv->mutex);
    }

    const char *result;
    if (!priv->open) {
        /* The serial port was closed behind our back. */
        errno = ENODEV;
        result = NULL;
    } else if (priv->waiting) {
        /* Timed out waiting for a response. */
        at_parser_reset(priv->at.parser);
        errno = ETIMEDOUT;
        result = NULL;
    } else {
        /* Response arrived. */
        result = priv->response;
    }

    /* Reset per-command settings. */
    priv->at.command_scanner = NULL;

    pthread_mutex_unlock(&priv->mutex);

    return result;
}

const char *at_command(struct at *at, const char *format, ...)
{
    struct at_unix *priv = (struct at_unix *) at;

    /* Build command string. */
    va_list ap;
    va_start(ap, format);
    char line[AT_COMMAND_LENGTH];
    int len = vsnprintf(line, sizeof(line)-1, format, ap);
    va_end(ap);

    /* Bail out if we run out of space. */
    if (len >= (int)(sizeof(line)-1)) {
        errno = ENOMEM;
        return NULL;
    }

    printf("> %s\n", line);

    /* Append modem-style newline. */
    line[len++] = '\r';

    /* Send the command. */
    return _at_command(priv, line, len);
}

const char *at_command_raw(struct at *at, const void *data, size_t size)
{
    struct at_unix *priv = (struct at_unix *) at;

    printf("> [%zu bytes]\n", size);

    return _at_command(priv, data, size);
}
#ifdef PARITY_ERR_SIMULATION
void at_sim_err(void)
{
	errflag = true;
}

void parityCheckTest(struct at *at)
{
	struct at_unix *priv = (struct at_unix *) at;
	for(int i=0; i<50; i++)
	{
		if((i%10)==9)
		{
			at_sim_err();
			printf("\nSimulate error\n");
			sleep(1);
		}
		const char *response  = at_command(priv, "AT+CGSN");
	}
}
#endif

void *at_reader_thread(void *arg)
{
    struct at_unix *priv = (struct at_unix *)arg;

    printf("at_reader_thread[%s]: starting\n", priv->devpath);

    while (true) {
        pthread_mutex_lock(&priv->mutex);

        /* Wait for the port descriptor to be valid. */
        while (priv->running && !priv->open)
            pthread_cond_wait(&priv->cond, &priv->mutex);

        if (!priv->running) {
            /* Time to die. */
            pthread_mutex_unlock(&priv->mutex);
            break;
        }

        /* Lock access to the port descriptor. */
        priv->busy = true;
        pthread_mutex_unlock(&priv->mutex);

        /* Attempt to read some data. */
        char ch;

        int result = read(priv->fd, &ch, 1);
        int why = errno;
        pthread_mutex_lock(&priv->mutex);
        /* Unlock access to the port descriptor. */
        priv->busy = false;
        /* Notify at_close() that the port is now free. */
        pthread_cond_signal(&priv->cond);
        pthread_mutex_unlock(&priv->mutex);

        if (result == 1) {
            /* Data received, feed the parser. */
            pthread_mutex_lock(&priv->mutex);
            at_parser_feed(priv->at.parser, &ch, 1);
            pthread_mutex_unlock(&priv->mutex);
        }
        else if (result == -1)
        {
            printf("at_reader_thread[%s]: %s\n", priv->devpath, strerror(why));
            if (why == EINTR)
            {
                continue;
            }
            else
                break;
        }
        else if (result == 3)
        {
            //Make sure that session is still open
            if(priv->open == true)
            {
               pthread_mutex_lock(&priv->mutex);
               priv->errParityCtr++;
               pthread_mutex_unlock(&priv->mutex);
#ifdef PARITY_ERR_SIMULATION
               struct termios attr;
               int rc = tcgetattr(priv->fd, &attr);
               int speed = cfgetispeed(&attr);
               rc = cfsetispeed(&attr,57600);
               attr.c_cflag &=  ~CSTOPB;
               tcsetattr(priv->fd, TCSANOW, &attr);
#endif
               continue;
            }

        }else {
            printf("at_reader_thread[%s]: received EOF\n", priv->devpath);
            break;
        }
    }

    printf("at_reader_thread[%s]: finished\n", priv->devpath);

    return NULL;
}

/* vim: set ts=4 sw=4 et: */
