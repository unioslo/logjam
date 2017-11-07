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

#include <sys/types.h>

#include <stdint.h>

#include <cryb/test.h>

#include <logjam/cirq.h>

static int numbers[] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
	0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
};


/***************************************************************************
 * Test cases
 */
static int
t_cirq_put_get_simple(char **desc CRYB_UNUSED, void *arg CRYB_UNUSED)
{
	cirq *q;
	size_t nput, nget, ndrop;
	int ret;

	ret = 1;
	q = cirq_create(7);
	t_assert(q != NULL);
	cirq_put(q, &numbers[9]);
	ret &= t_compare_sz(1, cirq_len(q));
	ret &= t_compare_i(numbers[9], *(int *)cirq_get(q, 0));
	ret &= t_compare_sz(0, cirq_len(q));
	cirq_stat(q, &nput, &nget, &ndrop, 0);
	ret &= t_compare_sz(1, nput) &
	    t_compare_sz(1, nget) &
	    t_compare_sz(0, ndrop);
	cirq_destroy(q);
	return (ret);
}

static int
t_cirq_put_get_full(char **desc CRYB_UNUSED, void *arg CRYB_UNUSED)
{
	cirq *q;
	size_t nput, nget, ndrop;
	int i, ret;

	ret = 1;
	q = cirq_create(7);
	t_assert(q != NULL);
	for (i = 0; i < 7; ++i)
		cirq_put(q, &numbers[i]);
	ret &= t_compare_sz(7, cirq_len(q));
	for (i = 0; i < 7; ++i)
		ret &= t_compare_i(numbers[i], *(int *)cirq_get(q, 0));
	ret &= t_compare_sz(0, cirq_len(q));
	cirq_stat(q, &nput, &nget, &ndrop, 0);
	ret &= t_compare_sz(7, nput) &
	    t_compare_sz(7, nget) &
	    t_compare_sz(0, ndrop);
	cirq_destroy(q);
	return (ret);
}

static int
t_cirq_put_get_overfull(char **desc CRYB_UNUSED, void *arg CRYB_UNUSED)
{
	cirq *q;
	size_t nput, nget, ndrop;
	int i, ret;

	ret = 1;
	q = cirq_create(7);
	t_assert(q != NULL);
	for (i = 0; i < 10; ++i)
		cirq_put(q, &numbers[i]);
	ret &= t_compare_sz(7, cirq_len(q));
	for (i = 3; i < 10; ++i)
		ret &= t_compare_i(numbers[i], *(int *)cirq_get(q, 0));
	ret &= t_compare_sz(0, cirq_len(q));
	cirq_stat(q, &nput, &nget, &ndrop, 0);
	ret &= t_compare_sz(10, nput) &
	    t_compare_sz(7, nget) &
	    t_compare_sz(3, ndrop);
	cirq_destroy(q);
	return (ret);
}


/***************************************************************************
 * Boilerplate
 */

static int
t_prepare(int argc CRYB_UNUSED, char *argv[] CRYB_UNUSED)
{

	t_add_test(t_cirq_put_get_simple, NULL, "simple put and get");
	t_add_test(t_cirq_put_get_full, NULL, "fill to capacity");
	t_add_test(t_cirq_put_get_overfull, NULL, "fill beyond capacity");
	return (0);
}

static void
t_cleanup(void)
{

}

int
main(int argc, char *argv[])
{

	t_main(t_prepare, t_cleanup, argc, argv);
}
