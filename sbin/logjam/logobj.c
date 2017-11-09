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

#include <stdint.h>
#include <stdlib.h>

#include <jansson.h>

#include <logjam/logobj.h>

lj_logobj *
lj_logobj_create(void)
{
	lj_logobj *lo;

	if ((lo = calloc(1, sizeof *lo)) == NULL ||
	    (lo->json = json_object()) == NULL)
		goto fail;
	return (lo);
fail:
	lj_logobj_destroy(lo);
	return (NULL);
}

void
lj_logobj_destroy(lj_logobj *lo)
{

	if (lo == NULL)
		return;
	json_decref(lo->json);
	free(lo);
}

int
lj_logobj_settime(lj_logobj *lo, uint64_t t)
{
	json_t *obj;

	/* for now, convert from microseconds to seconds */
	obj = json_integer(t / 1000000);
	return (json_object_set_new(lo->json, "timestamp", obj));
}

int
lj_logobj_setstr(lj_logobj *lo, const char *key, const char *value)
{

	return (json_object_set_new(lo->json, key, json_string(value)));
}

int
lj_logobj_setstrn(lj_logobj *lo, const char *key, const char *value, size_t len)
{
#if JANSSON_VERSION_HEX < 0x020700
	int ch, ret;

	ch = value[len];
	*(char *)(value + len) = '\0';
	ret = json_object_set_new(lo->json, key, json_string(value));
	*(char *)(value + len) = ch;
	return (ret);
#else
	return (json_object_set_new(lo->json, key, json_stringn(value, len)));
#endif
}
