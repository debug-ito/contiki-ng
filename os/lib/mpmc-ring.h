/*
 * Copyright (c) 2019, Toshiba Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * \file
 *         Multi-Producer Multi-Consumer lock-free ring buffer
 * \author
 *         Toshio Ito <toshio9.ito@toshiba.co.jp>
 *
 * mpmc-ring is analogous to ringbufindex, but it supports
 * multi-producer, multi-consumer scenarios. To do that, mpmc-ring
 * requires extra memory.
 *
 * mpmc-ring uses sys/atomic to arbitrate parallel accesses to the
 * queue. If your platform implements sys/atomic by deferring
 * interrupts, mpmc-queue also defers interrupts.
 *
 * To define and allocate a mpmc-ring object, use MPMC_RING
 * macro. For basic usage, see
 * examples/libs/mpmc-ring-interrupt/mpmc-ring-interrupt.c,
 * especially do_get and do_put functions.
 */

#ifndef _MPMC_RING_H_
#define _MPMC_RING_H_

#include "contiki.h"
#include "sys/atomic.h"
#include "sys/cc.h"

/*----------------------------------------------------------------------------------------*/

/*
 * (For debug) Size of entries to trace the behavior of a mpmc-ring.
 */
#ifdef MPMC_RING_CONF_DEBUG_TRACE_SIZE
#define MPMC_RING_DEBUG_TRACE_SIZE MPMC_RING_CONF_DEBUG_TRACE_SIZE
#else /* MPMC_RING_CONF_DEBUG_TRACE_SIZE */
#define MPMC_RING_DEBUG_TRACE_SIZE 0
#endif /* MPMC_RING_CONF_DEBUG_TRACE_SIZE */

#define MPMC_RING_DEBUG_TRACE_ENABLED ((MPMC_RING_DEBUG_TRACE_SIZE) > 0)

/*
 * (For debug/test) Delay (in counts) deliberately put in operations
 * of mpmc-ring. Useful to test parallel operations.
 */
#ifdef MPMC_RING_CONF_DEBUG_DELAY
#define MPMC_RING_DEBUG_DELAY MPMC_RING_CONF_DEBUG_DELAY
#else /* MPMC_RING_CONF_DEBUG_DELAY */
#define MPMC_RING_DEBUG_DELAY 0
#endif /* MPMC_RING_CONF_DEBUG_DELAY */

#define MPMC_RING_DEBUG_DELAY_ENABLED ((MPMC_RING_DEBUG_DELAY) > 0)

/*----------------------------------------------------------------------------------------*/

/**
 * Index managed by a mpmc_ring.
 */
typedef uint8_t mpmc_ring_index_t;

#if MPMC_RING_DEBUG_TRACE_ENABLED
enum mpmc_ring_trace_event {
  MPMC_RING_TRACE_EVENT_UNDEFINED = 0,
  MPMC_RING_TRACE_EVENT_EMPTY_TO_PUTTING,
  MPMC_RING_TRACE_EVENT_PUTTING_TO_OCCUPIED,
  MPMC_RING_TRACE_EVENT_OCCUPIED_TO_GETTING,
  MPMC_RING_TRACE_EVENT_GETTING_TO_EMPTY,
  MPMC_RING_TRACE_EVENT_NEXT_PUT_PTR,
  MPMC_RING_TRACE_EVENT_NEXT_GET_PTR,
};

/**
 * (For debug) A trace entry about behavior of mpmc_queue.
 */
struct mpmc_ring_trace_entry {
  mpmc_ring_index_t target;
  uint8_t event;
};
#endif /* MPMC_RING_DEBUG_TRACE_ENABLED */


/**
 * The multi-producer multi-consumer ring buffer.
 *
 * Do not declare and define struct mpmc_ring directly. Use
 * MPMC_RING macro instead.
 */
struct mpmc_ring {
  mpmc_ring_index_t put_ptr;
  mpmc_ring_index_t get_ptr;
  uint8_t * const state;
  const mpmc_ring_index_t mask;
#if MPMC_RING_DEBUG_TRACE_ENABLED
  struct mpmc_ring_trace_entry debug_trace[MPMC_RING_DEBUG_TRACE_SIZE];
  uint16_t debug_trace_next_put;
#endif /* MPMC_RING_DEBUG_TRACE_ENABLED */
};

/*----------------------------------------------------------------------------------------*/

/**
 * Declare and define a mpmc_ring.
 *
 * \param name Variable name of the struct mpmc_ring.
 *
 * \param size Size of the array that keeps the queue elements. Must
 * be power of 2, and 0 < size <= 256.
 */
#define MPMC_RING(name, size)                                           \
  static uint8_t CC_CONCAT(name,_state)[size];                          \
  static struct mpmc_ring name = { 0, 0, CC_CONCAT(name,_state), (size) - 1 };

/**
 * Initialize the mpmc_ring.
 */
void mpmc_ring_init(struct mpmc_ring *ring);

/**
 * Start putting an element to the queue. Every call to
 * mpmc_ring_put_begin must be finished by one and only call to
 * mpmc_ring_put_commit.
 *
 * \retval >=0 The index that the caller should use to put an
 * element.
 *
 * \retval <0 The queue is full. The caller cannot put an element.
 */
int mpmc_ring_put_begin(struct mpmc_ring *ring);

/**
 * Finish putting an element to the queue.
 *
 * \param ring The mpmc_ring object.
 *
 * \param index The index obtained by the matching call to
 * mpmc_ring_put_begin.
 *
 */
void mpmc_ring_put_commit(struct mpmc_ring *ring, mpmc_ring_index_t index);

/**
 * Start getting an element from the queue. Every call to
 * mpmc_ring_get_begin must be finished by one and only call to
 * mpmc_ring_get_commit.
 *
 * \retval >=0 The index that the caller should use to get an
 * element.
 *
 * \retval <0 The queue is empty. The caller cannot get an element.
 */
int mpmc_ring_get_begin(struct mpmc_ring *ring);

/**
 * Finish getting an element to the queue.
 *
 * \param ring The mpmc_ring object.
 *
 * \param index The index obtained by the matching call to
 * mpmc_ring_get_begin.
 *
 */
void mpmc_ring_get_commit(struct mpmc_ring *ring, mpmc_ring_index_t index);

/**
 * \return Number of elements currently in the queue.
 */
int mpmc_ring_elements(const struct mpmc_ring *ring);

/**
 * \return Non-zero if the queue is empty. Zero otherwise.
 */
int mpmc_ring_empty(const struct mpmc_ring *ring);


#if MPMC_RING_DEBUG_TRACE_ENABLED
/**
 * (For debug) Print the trace entries of the mpmc_ring. It prints
 * the newest trace entry first, followed by older entries.
 */
void mpmc_ring_print_debug_trace(const struct mpmc_ring *ring);
#else /* MPMC_RING_DEBUG_TRACE_ENABLED */
#define mpmp_ring_print_debug_trace(r)
#endif /* MPMC_RING_DEBUG_TRACE_ENABLED */


#endif /* _MPMC_RING_H_ */
