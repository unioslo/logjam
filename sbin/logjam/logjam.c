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
#include <logjam/log.h>
#include <logjam/flume.h>
#include <logjam/reader.h>
#include <logjam/parser.h>
#include <logjam/sender.h>

#define CIRQ_SIZE 1024

static volatile bool quit;

static cirq *ll_cirq;
static cirq *lo_cirq;

static pthread_t rthr;
static pthread_t pthr;
static pthread_t sthr;

static void *
rthr_main(void *arg)
{
	lj_reader_ctx *ctx = arg;
	lj_logline *ll;

	while (!quit) {
		if ((ll = ctx->reader->read(ctx)) == NULL) {
			if (errno != EAGAIN)
				break;
			usleep(1000000);
			continue;
		}
		if ((ll = cirq_put(ll_cirq, ll)) != NULL)
			free(ll);
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
		if ((ll = cirq_get(ll_cirq, 1000000)) == NULL) {
			if (errno != ETIMEDOUT)
				break;
			continue;
		}
		if ((lo = ctx->parser->parse(ctx, ll)) != NULL) {
			if ((lo = cirq_put(lo_cirq, lo)) != NULL)
				lj_logobj_destroy(lo);
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
		if ((lo = cirq_get(lo_cirq, 1000000)) == NULL) {
			if (errno != ETIMEDOUT)
				break;
			continue;
		}
		ctx->sender->send(ctx, lo);
		lj_logobj_destroy(lo);
	}
	return (NULL);
}

void
logjam(void)
{
	lj_flume *flume;
	int r;

	signal(SIGPIPE, SIG_IGN);
	quit = false;

	if ((flume = lj_configure(lj_config_file)) == NULL)
		exit(1);

	if ((ll_cirq = cirq_create(CIRQ_SIZE)) == NULL)
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

	while (usleep(10000000) == 0)
		/* nothing */ ;

	quit = true;
	pthread_join(rthr, NULL);
	pthread_join(pthr, NULL);
	pthread_join(sthr, NULL);
	lj_flume_fini(flume);
}
