#include <string.h>
#include "lib/assert.h"
#include "sys/memory-barrier.h"
#include "sys/critical.h"

#include "mpmc-ring.h"

/*----------------------------------------------------------------------------------------*/

static inline uint8_t
elems(uint8_t put_index, uint8_t get_index, uint8_t mask)
{
  return (put_index - get_index) & mask;
}

static inline bool
is_empty(uint8_t put_index, uint8_t get_index, uint8_t mask)
{
  return elems(put_index, get_index, mask) == 0;
}

static inline bool
is_full(uint8_t put_index, uint8_t get_index, uint8_t mask)
{
  return elems(put_index, get_index, mask) == mask;
}

static inline uint8_t
next(uint8_t i, uint8_t mask)
{
  return (i + 1) & mask;
}

/*----------------------------------------------------------------------------------------*/

void
mpmc_ring_init(struct mpmc_ring *ring)
{
  uint8_t i;
  ring->put_ptr = 0;
  ring->get_ptr = 0;
  for(i = 0 ; i <= ring->mask ; i++) {
    ring->sequences[i] = i;
  }
}

int
mpmc_ring_put_begin(struct mpmc_ring *ring, mpmc_ring_index_t *got_index)
{
  // TODO
  return 0;
}

//// int
//// mpmc_ring_put_begin(struct mpmc_ring *ring)
//// {
////   mpmc_ring_index_t tmp_put;
////   mpmc_ring_index_t now_get;
////   assert(ring != NULL);
////   now_get = ring->get_ptr;
////   tmp_put = ring->put_ptr;
////   memory_barrier();
////   debug_delay(1);
////   while(!is_full(tmp_put, now_get, ring->mask)) {
////     debug_delay(2);
////     if(atomic_cas_uint8(&ring->state[tmp_put], MPMC_RING_EMPTY, MPMC_RING_PUTTING)) {
////       add_debug_trace(ring, tmp_put, MPMC_RING_TRACE_EVENT_EMPTY_TO_PUTTING);
////       return tmp_put;
////     } else {
////       /* Failed to update the state */
////       memory_barrier();
////       debug_delay(3);
////       if(ring->state[tmp_put] == MPMC_RING_EMPTY) {
////         /*
////          * Looks like no one succeeded to update the state. Retry at
////          * the same index.
////          */
////         ;
////       } else {
////         /*
////          * It's not EMPTY in the first place, or someone else
////          * updated the state. Try the next index.
////          */
////         tmp_put = next(tmp_put, ring->mask);
////       }
////     }
////   }
////   return -1;
//// }

void
mpmc_ring_put_commit(struct mpmc_ring *ring, const mpmc_ring_index_t *index)
{
  // TODO
}

//// void
//// mpmc_ring_put_commit(struct mpmc_ring *ring, mpmc_ring_index_t index)
//// {
////   mpmc_ring_index_t now_get;
////   assert(ring != NULL);
////   assert(ring->state[index] == MPMC_RING_PUTTING);
////   
////   ring->state[index] = MPMC_RING_OCCUPIED;
////   add_debug_trace(ring, index, MPMC_RING_TRACE_EVENT_PUTTING_TO_OCCUPIED);
////   debug_delay(4);
////   now_get = ring->get_ptr;
////   while(1) {
////     mpmc_ring_index_t tmp_put;
////     debug_delay(5);
////     tmp_put = ring->put_ptr;
////     memory_barrier();
////     if(ring->state[tmp_put] == MPMC_RING_OCCUPIED && !is_full(tmp_put, now_get, ring->mask)) {
////       debug_delay(6);
////       if(atomic_cas_uint8(&ring->put_ptr, tmp_put, next(tmp_put, ring->mask))) {
////         add_debug_trace(ring, tmp_put, MPMC_RING_TRACE_EVENT_NEXT_PUT_PTR);
////       }
////     } else {
////       break;
////     }
////   }
//// }

int
mpmc_ring_get_begin(struct mpmc_ring *ring, mpmc_ring_index_t *got_index)
{
  // TODO
  return 0;
}

void
mpmc_ring_get_commit(struct mpmc_ring *ring, const mpmc_ring_index_t *index)
{
  // TODO
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

