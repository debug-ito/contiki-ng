#include <string.h>
#include "lib/assert.h"
#include "sys/memory-barrier.h"
#include "sys/critical.h"

#include "mpmc-ring.h"

/*----------------------------------------------------------------------------------------*/
/**
 * State of a slot in the queue. Stored in ring->state array.
 */
enum mpmc_ring_state {
  MPMC_RING_EMPTY = 0, /**< The slot is empty. Transitions to PUTTING by put_begin. */
  MPMC_RING_PUTTING,   /**< A producer is writing to the slot right now. Transitions to OCCUPIED by put_commit. */
  MPMC_RING_GETTING,   /**< A consumer is reading the slot right now. Transitions to EMPTY by get_commit. */
  MPMC_RING_OCCUPIED,  /**< The slot is occupied. Transitions to GETTING by get_begin. */
};

/*
 * Internal of mpmc_ring
 * 
 * The figure below shows an example of the `state` array of
 * mpmc_ring.
 *
 *      0 1 2 3 4 5 6 7 8 9 a b c d e f
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |G|G|E|O|O|O|O|O|P|P|P|O|E|E|E|E|
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *      ^               ^
 *      get_ptr         put_ptr
 *
 * The state array is divided into two regions; the O region
 * (get_ptr <= i < put_ptr) and the E region (put_ptr <= i < get_ptr).
 * 
 * Slots in the O region are basically OCCUPIED, can be GETTING or
 * EMPTY temporarily but never PUTTING. Slots in the E region are
 * basically EMPTY, can be PUTTING or OCCUPIED temporarily but never
 * GETTING. If get_ptr == put_ptr, the whole queue is in E region
 * and there is no O region.
 *
 * get_ptr points to the lowest GETTING slot if there is any GETTING
 * slots in the queue. Otherwise, get_ptr points to the lowest
 * OCCUPIED slot if there is any OCCUPIED slots in the
 * queue. Otherwise, get_ptr must be equal to put_ptr.
 *
 * put_ptr points to the lowest PUTTING slot if there is any PUTTING
 * slots in the queue. Otherwise, put_ptr points to the lowest EMPTY
 * slot. Note that there is always at least one EMPTY slot in the
 * queue. So, if get_ptr == put_ptr, this means the queue is
 * (almost) empty.
 *
 * GETTING slots can transition to EMPTY in out-of-order
 * fashion. For example, the slot 2 in the above figure must have
 * transitioned to GETTING after slots 0 and 1 became GETTING. Then,
 * the slot 2 transitioned to EMPTY before slots 0 and 1. After
 * slots 0 and 1 both transition to EMPTY, get_ptr will move to slot
 * 3. Similar discussion applies to PUTTING slots and put_ptr.
 *
 */

/*----------------------------------------------------------------------------------------*/

static inline mpmc_ring_index_t
elems(mpmc_ring_index_t put_index, mpmc_ring_index_t get_index, mpmc_ring_index_t mask)
{
  return (put_index - get_index) & mask;
}

static inline bool
is_empty(mpmc_ring_index_t put_index, mpmc_ring_index_t get_index, mpmc_ring_index_t mask)
{
  return elems(put_index, get_index, mask) == 0;
}

static inline bool
is_full(mpmc_ring_index_t put_index, mpmc_ring_index_t get_index, mpmc_ring_index_t mask)
{
  return elems(put_index, get_index, mask) == mask;
}

static inline mpmc_ring_index_t
next(mpmc_ring_index_t i, mpmc_ring_index_t mask)
{
  return (i + 1) & mask;
}

/*----------------------------------------------------------------------------------------*/

#if MPMC_RING_DEBUG_TRACE_ENABLED
static inline void
add_debug_trace(struct mpmc_ring *ring, mpmc_ring_index_t index, uint8_t event)
{
  int_master_status_t s = critical_enter();
  ring->debug_trace[ring->debug_trace_next_put].target = index;
  ring->debug_trace[ring->debug_trace_next_put].event = event;
  ring->debug_trace_next_put = (ring->debug_trace_next_put + 1) % MPMC_RING_DEBUG_TRACE_SIZE;
  critical_exit(s);
}

static inline uint16_t
prev_trace_index(uint16_t index)
{
  return index == 0 ? (MPMC_RING_DEBUG_TRACE_SIZE - 1) : (index - 1);
}

static const char *trace_event_desc[] =
  {
    "UNDEFINED",
    "EMPTY_TO_PUTTING",
    "PUTTING_TO_OCCUPIED",
    "OCCUPIED_TO_GETTING",
    "GETTING_TO_EMPTY",
    "NEXT_PUT_PTR",
    "NEXT_GET_PTR",
  };

void
mpmc_ring_print_debug_trace(const struct mpmc_ring *ring)
{
  uint16_t end = ring->debug_trace_next_put;
  uint16_t i;
  uint16_t n = 0;
  for(i = prev_trace_index(end) ; i != end ; i = prev_trace_index(i), n++) {
    const struct mpmc_ring_trace_entry *e = &ring->debug_trace[i];
    if(e->event == MPMC_RING_TRACE_EVENT_UNDEFINED) break;
    printf("[MPMC_RING_TRACE] %5u target:%-3u event:%u(%s)\n",
           n, e->target, e->event, trace_event_desc[e->event]);
  }
}
#else /* MPMC_RING_DEBUG_TRACE_ENABLED */
#define add_debug_trace(r,i,e)
#endif /* MPMC_RING_DEBUG_TRACE_ENABLED */


#if MPMC_RING_DEBUG_DELAY_ENABLED
static void
debug_delay(void)
{
  volatile uint32_t i;
  for(i = 0 ; i < MPMC_RING_DEBUG_DELAY ; i++) {
    ;
  }
}
#else /* MPMC_RING_DEBUG_DELAY_ENABLED */
#define debug_delay()
#endif /* MPMC_RING_DEBUG_DELAY_ENABLED */


/*----------------------------------------------------------------------------------------*/

void
mpmc_ring_init(struct mpmc_ring *ring)
{
  ring->put_ptr = 0;
  ring->get_ptr = 0;
  memset(ring->state, 0, (size_t)ring->mask + 1);
#if MPMC_RING_DEBUG_TRACE_ENABLED
  memset(ring->debug_trace, 0, sizeof(ring->debug_trace));
  ring->debug_trace_next_put = 0;
#endif /* MPMC_RING_DEBUG_TRACE_ENABLED */
}

int
mpmc_ring_put_begin(struct mpmc_ring *ring)
{
  mpmc_ring_index_t tmp_put;
  mpmc_ring_index_t now_get;
  assert(ring != NULL);
  now_get = ring->get_ptr;
  tmp_put = ring->put_ptr;
  memory_barrier();
  debug_delay();
  while(!is_full(tmp_put, now_get, ring->mask)) {
    debug_delay();
    if(atomic_cas_uint8(&ring->state[tmp_put], MPMC_RING_EMPTY, MPMC_RING_PUTTING)) {
      add_debug_trace(ring, tmp_put, MPMC_RING_TRACE_EVENT_EMPTY_TO_PUTTING);
      return tmp_put;
    } else {
      /* Failed to update the state */
      memory_barrier();
      debug_delay();
      if(ring->state[tmp_put] == MPMC_RING_EMPTY) {
        /*
         * Looks like no one succeeded to update the state. Retry at
         * the same index.
         */
        ;
      } else {
        /*
         * It's not EMPTY in the first place, or someone else
         * updated the state. Try the next index.
         */
        tmp_put = next(tmp_put, ring->mask);
      }
    }
  }
  return -1;
}

void
mpmc_ring_put_commit(struct mpmc_ring *ring, mpmc_ring_index_t index)
{
  mpmc_ring_index_t now_get;
  assert(ring != NULL);
  assert(ring->state[index] == MPMC_RING_PUTTING);
  
  ring->state[index] = MPMC_RING_OCCUPIED;
  add_debug_trace(ring, index, MPMC_RING_TRACE_EVENT_PUTTING_TO_OCCUPIED);
  debug_delay();
  now_get = ring->get_ptr;
  while(1) {
    mpmc_ring_index_t tmp_put;
    debug_delay();
    tmp_put = ring->put_ptr;
    memory_barrier();
    if(ring->state[tmp_put] == MPMC_RING_OCCUPIED && !is_full(tmp_put, now_get, ring->mask)) {
      debug_delay();
      if(atomic_cas_uint8(&ring->put_ptr, tmp_put, next(tmp_put, ring->mask))) {
        add_debug_trace(ring, tmp_put, MPMC_RING_TRACE_EVENT_NEXT_PUT_PTR);
      }
    } else {
      break;
    }
  }
}

int
mpmc_ring_get_begin(struct mpmc_ring *ring)
{
  /* The dual of mpmc_ring_put_begin. */
  mpmc_ring_index_t tmp_get;
  mpmc_ring_index_t now_put;
  assert(ring != NULL);
  now_put = ring->put_ptr;
  tmp_get = ring->get_ptr;
  memory_barrier();
  debug_delay();
  while(!is_empty(now_put, tmp_get, ring->mask)) {
    debug_delay();
    if(atomic_cas_uint8(&ring->state[tmp_get], MPMC_RING_OCCUPIED, MPMC_RING_GETTING)) {
      add_debug_trace(ring, tmp_get, MPMC_RING_TRACE_EVENT_OCCUPIED_TO_GETTING);
      return tmp_get;
    } else {
      memory_barrier();
      debug_delay();
      if(ring->state[tmp_get] == MPMC_RING_OCCUPIED) {
        ;
      } else {
        tmp_get = next(tmp_get, ring->mask);
      }
    }
  }
  return -1;
}

void
mpmc_ring_get_commit(struct mpmc_ring *ring, mpmc_ring_index_t index)
{
  /* The dual of mpmc_ring_put_commit. */
  mpmc_ring_index_t now_put;
  assert(ring != NULL);
  assert(ring->state[index] == MPMC_RING_GETTING);
  
  ring->state[index] = MPMC_RING_EMPTY;
  add_debug_trace(ring, index, MPMC_RING_TRACE_EVENT_GETTING_TO_EMPTY);
  debug_delay();
  now_put = ring->put_ptr;
  while(1) {
    mpmc_ring_index_t tmp_get;
    debug_delay();
    tmp_get = ring->get_ptr;
    memory_barrier();
    if(ring->state[tmp_get] == MPMC_RING_EMPTY && !is_empty(now_put, tmp_get, ring->mask)) {
      debug_delay();
      if(atomic_cas_uint8(&ring->get_ptr, tmp_get, next(tmp_get, ring->mask))) {
        add_debug_trace(ring, tmp_get, MPMC_RING_TRACE_EVENT_NEXT_GET_PTR);
      }
    } else {
      break;
    }
  }
}

int
mpmc_ring_elements(const struct mpmc_ring *ring)
{
  assert(ring != NULL);
  return elems(ring->put_ptr, ring->get_ptr, ring->mask);
}

int
mpmc_ring_empty(const struct mpmc_ring *ring)
{
  return mpmc_ring_elements(ring) == 0;
}
