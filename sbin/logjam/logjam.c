/*-
 * Copyright (c) 2017 The University of Oslo
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <logjam/cirq.h>
#include <logjam/config.h>
#include <logjam/debug.h>
#include <logjam/log.h>
#include <logjam/flume.h>
#include <logjam/reader.h>
#include <logjam/parser.h>
#include <logjam/sender.h>

#define CIRQ_SIZE 1024

static volatile sig_atomic_t sigusr1;
static volatile sig_atomic_t sigusr2;
static volatile sig_atomic_t sigterm;

static volatile bool quit;

static cirq *li_cirq;
static cirq *lo_cirq;

static pthread_t rthr;
static pthread_t pthr;
static pthread_t sthr;

static void
sig_handler(int signo)
{

	switch (signo) {
	case SIGINT:
	case SIGTERM:
		sigterm++;
		break;
	case SIGUSR1:
		sigusr1++;
		break;
	case SIGUSR2:
		sigusr2++;
		break;
	}
}

static void *
rthr_main(void *arg)
{
	lj_reader_ctx *ctx = arg;
	lj_logline *ll;

	while (!quit) {
		if ((ll = ctx->reader->read(ctx)) == NULL) {
			if (errno != EAGAIN)
				break;
			usleep(100000);
			continue;
		}
		if ((ll = cirq_put(li_cirq, ll)) != NULL) {
			/* destroy displaced entry */
			free(ll);
		}
	}
	return (NULL);
}

static void *
pthr_main(void *arg)
{
	lj_parser_ctx *ctx = arg;
	lj_logline *ll;
	lj_logobj *lo;

	while (!quit) {
		if ((ll = cirq_get(li_cirq, 100000)) == NULL) {
			if (errno != ETIMEDOUT)
				break;
			continue;
		}
		if ((lo = ctx->parser->parse(ctx, ll)) != NULL) {
			if ((lo = cirq_put(lo_cirq, lo)) != NULL) {
				/* destroy displaced entry */
				lj_logobj_destroy(lo);
			}
		}
		free(ll);
	}
	return (NULL);
}

static void *
sthr_main(void *arg)
{
	lj_sender_ctx *ctx = arg;
	lj_logobj *lo;

	(void)ctx;
	while (!quit) {
		if ((lo = cirq_get(lo_cirq, 100000)) == NULL) {
			if (errno != ETIMEDOUT)
				break;
			continue;
		}
		ctx->sender->send(ctx, lo);
		lj_logobj_destroy(lo);
	}
	return (NULL);
}

static void
logstats(int clear)
{
	uintmax_t nput, nget, ndrop;

	cirq_stat(li_cirq, &nput, &nget, &ndrop, clear);
	lj_debug(1, "i: put %zu get %zu drop %zu\n", nput, nget, ndrop);
	cirq_stat(lo_cirq, &nput, &nget, &ndrop, clear);
	lj_debug(1, "o: put %zu get %zu drop %zu\n", nput, nget, ndrop);
}

void
logjam(void)
{
	lj_flume *flume;
	int r;

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, &sig_handler);
	signal(SIGTERM, &sig_handler);
	signal(SIGUSR1, &sig_handler);
	signal(SIGUSR2, &sig_handler);

	if ((flume = lj_configure(lj_config_file)) == NULL)
		exit(1);

	if ((li_cirq = cirq_create(CIRQ_SIZE)) == NULL)
		err(1, "failed to create input cirq");
	if ((lo_cirq = cirq_create(CIRQ_SIZE)) == NULL)
		err(1, "failed to create output cirq");

	if ((r = pthread_create(&rthr, NULL, rthr_main, flume->rctx)) != 0) {
		errno = r;
		err(1, "failed to start reader thread");
	}

	if ((r = pthread_create(&pthr, NULL, pthr_main, flume->pctx)) != 0) {
		errno = r;
		err(1, "failed to start parser thread");
	}

	if ((r = pthread_create(&sthr, NULL, sthr_main, flume->sctx)) != 0) {
		errno = r;
		err(1, "failed to start sender thread");
	}

	while (!quit) {
		usleep(100000);
		if (sigterm > 0) {
			quit = true;
			sigusr1++;
			sigterm = 0;
		}
		if (sigusr1 + sigusr2 > 0) {
			logstats(sigusr2);
			sigusr1 = sigusr2 = 0;
		}
	}

	pthread_join(rthr, NULL);
	pthread_join(pthr, NULL);
	pthread_join(sthr, NULL);
	lj_flume_fini(flume);
}
