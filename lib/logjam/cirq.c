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

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#include <logjam/cirq.h>

struct cirq {
	pthread_mutex_t	 mutex;
	pthread_cond_t	 cond;
	size_t		 size;
	unsigned int	 ridx;
	unsigned int	 widx;
	size_t		 nput;
	size_t		 nget;
	size_t		 ndrop;
	void		*obj[];
};

/*
 * Create a cirq with room for the specified number of objects.
 *
 * The cirq has a mutex for protection, a condition variable to signal
 * waiting readers, a size, an array of pointers to objects, and read and
 * write indices.  There are two cases where these can point to the same
 * location: if the cirq is empty and if it is full.  The difference is
 * that in the first case, the value stored at that location is NULL.
 */
cirq *
cirq_create(size_t nobj)
{
	cirq *c;

	if (nobj < 2) {
		errno = EINVAL;
		return (NULL);
	}
	if ((c = calloc(1, sizeof *c + nobj * sizeof *c->obj)) == NULL)
		return (NULL);
	c->size = nobj;
	if (pthread_mutex_init(&c->mutex, NULL) != 0)
		goto fail;
	if (pthread_cond_init(&c->cond, NULL) != 0)
		goto fail;
	return (c);
fail:
	cirq_destroy(c);
	return (NULL);
}

/*
 * Destroy a cirq.
 */
void
cirq_destroy(cirq *c)
{

	if (c != NULL) {
		pthread_cond_destroy(&c->cond);
		pthread_mutex_destroy(&c->mutex);
		free(c);
	}
}

/*
 * Return the number of objects in the cirq.
 */
size_t
cirq_len(cirq *c)
{
	size_t len;

	assert(c != NULL);
	pthread_mutex_lock(&c->mutex);
	assert(c->ridx < c->size);
	assert(c->widx < c->size);
	if (c->widx == c->ridx) {
		len = c->obj[c->ridx] == NULL ? 0 : c->size;
	} else {
		len = (c->widx + c->size - c->ridx) % c->size;
	}
	pthread_mutex_unlock(&c->mutex);
	return (len);
}

/*
 * Place an object onto the cirq.  If the cirq is full, the new object
 * will displace the oldest one.
 */
void *
cirq_put(cirq *c, void *obj)
{
	unsigned int idx;
	void *old;

	assert(c != NULL);
	assert(obj != NULL);
	pthread_mutex_lock(&c->mutex);
	assert(c->ridx < c->size);
	assert(c->widx < c->size);
	idx = c->widx;
	c->widx = (idx + 1) % c->size;
	/* if the cirq is full, advance the read index */
	if ((old = c->obj[idx]) != NULL) {
		assert(c->ridx == idx);
		c->ridx = c->widx;
		c->ndrop++;
	}
	c->obj[idx] = obj;
	c->nput++;
	pthread_cond_signal(&c->cond);
	pthread_mutex_unlock(&c->mutex);
	return (old);
}

/*
 * Retrieve and return the oldest object in the cirq.  If the cirq is
 * empty, wait for the specified amount of time (in microseconds) for data
 * to arrive.  Returns NULL if the cirq is still empty after the
 * deadline or if an error occurs; errno will be ETIMEDOUT in the former
 * case and another non-zero value in the latter.
 */
void *
cirq_get(cirq *c, unsigned int timeout)
{
	struct timespec ts;
	void *obj;
	int r;

	assert(c != NULL);
	pthread_mutex_lock(&c->mutex);
	assert(c->ridx < c->size);
	assert(c->widx < c->size);
	r = 0;
	if (c->obj[c->ridx] == NULL) {
		assert(c->widx == c->ridx);
		/* compute deadline */
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += timeout / 1000000;
		ts.tv_nsec += (timeout % 1000000) * 1000;
		ts.tv_nsec %= 1000000000;
		/* loop until data appears or we time out */
		do {
			r = pthread_cond_timedwait(&c->cond, &c->mutex, &ts);
		} while (r == 0 && c->obj[c->ridx] == NULL);
	}
	errno = r;
	/* if data appeared, remove it and advance the read pointer */
	if ((obj = c->obj[c->ridx]) != NULL) {
		c->obj[c->ridx] = NULL;
		c->ridx = (c->ridx + 1) % c->size;
		c->nget++;
	}
	pthread_mutex_unlock(&c->mutex);
	return (obj);
}

/*
 * Return and optionally clear statistics.
 */
void
cirq_stat(cirq *c, size_t *nput, size_t *nget, size_t *ndrop,
    int clear)
{

	pthread_mutex_lock(&c->mutex);
	*nput = c->nput;
	*nget = c->nget;
	*ndrop = c->ndrop;
	if (clear)
		c->nput = c->nget = c->ndrop = 0;
	pthread_mutex_unlock(&c->mutex);
}
