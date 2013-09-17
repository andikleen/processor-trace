/*
 * Copyright (c) 2013, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "pt_state.h"
#include "pt_packet_decode.h"

#include "pt_opcode.h"
#include "pt_decode.h"
#include "pt_error.h"
#include "pt_config.h"

#include <stdlib.h>
#include <string.h>


struct pt_decoder *pt_alloc_decoder(const struct pt_config *config)
{
	struct pt_decoder *decoder;

	if (!config)
		return NULL;

	if (config->size != sizeof(*config))
		return NULL;

	if (!config->begin || !config->end)
		return NULL;

	if (config->end < config->begin)
		return NULL;

	decoder = (struct pt_decoder *) calloc(sizeof(*decoder), 1);
	if (!decoder)
		return NULL;

	decoder->config = *config;

	pt_last_ip_init(&decoder->ip);
	pt_tnt_cache_init(&decoder->tnt);

	return decoder;
}

void pt_free_decoder(struct pt_decoder *decoder)
{
	free(decoder);
}

uint64_t pt_get_decoder_pos(struct pt_decoder *decoder)
{
	const uint8_t *raw, *begin;

	if (!decoder)
		return 0;

	raw = decoder->pos;
	if (!raw)
		return 0;

	begin = pt_begin(decoder);
	if (!begin)
		return 0;

	return raw - begin;
}

uint64_t pt_get_decoder_sync(struct pt_decoder *decoder)
{
	const uint8_t *sync, *begin;

	if (!decoder)
		return 0;

	sync = decoder->sync;
	if (!sync)
		return 0;

	begin = pt_begin(decoder);
	if (!begin)
		return 0;

	return sync - begin;
}

const uint8_t *pt_get_decoder_raw(const struct pt_decoder *decoder)
{
	if (!decoder)
		return NULL;

	return decoder->pos;
}

const uint8_t *pt_get_decoder_begin(const struct pt_decoder *decoder)
{
	if (!decoder)
		return NULL;

	return pt_begin(decoder);
}

const uint8_t *pt_get_decoder_end(const struct pt_decoder *decoder)
{
	if (!decoder)
		return NULL;

	return pt_end(decoder);
}

int pt_status_flags(struct pt_decoder *decoder)
{
	const struct pt_decoder_function *dfun;
	int flags = 0;

	if (!decoder)
		return -pte_internal;

	dfun = decoder->next;
	if (dfun) {
		if (dfun->flags & pdff_event)
			flags |= pts_event_pending;

		if (dfun->flags & pdff_psbend)
			if (pt_event_pending(decoder, evb_psbend))
				flags |= pts_event_pending;

		if (dfun->flags & pdff_tip)
			if (pt_event_pending(decoder, evb_tip))
				flags |= pts_event_pending;

		if (dfun->flags & pdff_fup)
			if (pt_event_pending(decoder, evb_fup))
				flags |= pts_event_pending;
	}

	return flags;
}

void pt_reset(struct pt_decoder *decoder)
{
	int evb;

	if (!decoder)
		return;

	decoder->flags = 0;
	decoder->event = NULL;
	decoder->tsc = 0;

	pt_last_ip_init(&decoder->ip);
	pt_tnt_cache_init(&decoder->tnt);

	for (evb = 0; evb < evb_max; ++evb)
		pt_discard_events(decoder, evb);
}

static inline uint8_t pt_queue_inc(uint8_t idx)
{
	idx += 1;
	idx %= evb_max_pend;

	return idx;
}

struct pt_event *pt_enqueue_event(struct pt_decoder *decoder,
				  enum pt_event_binding evb)
{
	struct pt_event *ev;
	uint8_t begin, end, gap;

	if (!decoder)
		return NULL;

	if (evb_max <= evb)
		return NULL;

	begin = decoder->ev_begin[evb];
	end = decoder->ev_end[evb];

	if (evb_max_pend <= begin)
		return NULL;

	if (evb_max_pend <= end)
		return NULL;

	ev = &decoder->ev_pend[evb][end];

	end = pt_queue_inc(end);
	gap = pt_queue_inc(end);

	/* Leave a gap so we don't overwrite the last dequeued event. */
	if (begin == gap)
		return NULL;

	decoder->ev_end[evb] = end;

	/* This is not strictly necessary. */
	(void) memset(ev, 0, sizeof(*ev));

	return ev;
}

struct pt_event *pt_dequeue_event(struct pt_decoder *decoder,
				  enum pt_event_binding evb)
{
	struct pt_event *ev;
	uint8_t begin, end;

	if (!decoder)
		return NULL;

	if (evb_max <= evb)
		return NULL;

	begin = decoder->ev_begin[evb];
	end = decoder->ev_end[evb];

	if (evb_max_pend <= begin)
		return NULL;

	if (evb_max_pend <= end)
		return NULL;

	if (begin == end)
		return NULL;

	ev = &decoder->ev_pend[evb][begin];

	decoder->ev_begin[evb] = pt_queue_inc(begin);

	return ev;
}

void pt_discard_events(struct pt_decoder *decoder,
		       enum pt_event_binding evb)
{
	if (!decoder)
		return;

	if (evb_max <= evb)
		return;

	decoder->ev_begin[evb] = 0;
	decoder->ev_end[evb] = 0;
}

int pt_event_pending(struct pt_decoder *decoder,
		     enum pt_event_binding evb)
{
	uint8_t begin, end;

	if (!decoder)
		return -pte_internal;

	if (evb_max <= evb)
		return -pte_internal;

	begin = decoder->ev_begin[evb];
	end = decoder->ev_end[evb];

	if (evb_max_pend <= begin)
		return -pte_internal;

	if (evb_max_pend <= end)
		return -pte_internal;

	return begin != end;
}

struct pt_event *pt_find_event(struct pt_decoder *decoder,
			       enum pt_event_type type,
			       enum pt_event_binding evb)
{
	uint8_t begin, end;

	if (!decoder)
		return NULL;

	if (evb_max <= evb)
		return NULL;

	begin = decoder->ev_begin[evb];
	end = decoder->ev_end[evb];

	if (evb_max_pend <= begin)
		return NULL;

	if (evb_max_pend <= end)
		return NULL;

	for (; begin != end; begin = pt_queue_inc(begin)) {
		struct pt_event *ev;

		ev = &decoder->ev_pend[evb][begin];
		if (ev->type == type)
			return ev;
	}

	return NULL;
}

int pt_advance(struct pt_decoder *decoder, int size)
{
	const uint8_t *pos;

	if (!decoder)
		return -pte_invalid;

	pos = decoder->pos + size;

	if (pos < decoder->config.begin)
		return -pte_eos;

	if (decoder->config.end < pos)
		return -pte_eos;

	decoder->pos = pos;

	return 0;
}