#include <string.h>
#include "lib/assert.h"
#include "sys/memory-barrier.h"
#include "sys/critical.h"

#include "mpmc-ring.h"

/*----------------------------------------------------------------------------------------*/

void
mpmc_ring_init(mpmc_ring_t *ring)
{
  uint8_t i;
  ring->put_pos = 0;
  ring->get_pos = 0;
  for(i = 0 ; i <= ring->mask ; i++) {
    ring->sequences[i] = i;
  }
}

int
mpmc_ring_put_begin(mpmc_ring_t *ring, mpmc_ring_index_t *got_index)
{
  uint8_t pos;
  uint8_t index;

  assert(ring != NULL);
  assert(got_index != NULL);
  
  pos = ring->put_pos;
  memory_barrier();
  while(1) {
    int8_t dif;
    index = pos & ring->mask;
    dif = (int8_t)(ring->sequences[index]) - (int8_t)pos;
    memory_barrier();
    if(dif == 0) {
      if(atomic_cas_uint8(&ring->put_pos, pos, pos + 1)) {
        break;
      }
    } else if(dif < 0) {
      return 0;
    } else {
      pos = ring->put_pos;
      memory_barrier();
    }
  }
  got_index->i = index;
  got_index->_pos = pos;
  return 1;
}

void
mpmc_ring_put_commit(mpmc_ring_t *ring, const mpmc_ring_index_t *index)
{
  assert(ring != NULL);
  assert(index != NULL);

  ring->sequences[index->i] = index->_pos + 1;
}

int
mpmc_ring_get_begin(mpmc_ring_t *ring, mpmc_ring_index_t *got_index)
{
  /* Basically the dual of put_begin */
  uint8_t pos;
  uint8_t index;

  assert(ring != NULL);
  assert(got_index != NULL);
  
  pos = ring->get_pos;
  memory_barrier();
  while(1) {
    int8_t dif;
    index = pos & ring->mask;
    dif = (int8_t)(ring->sequences[index]) - (int8_t)(pos + 1);
    memory_barrier();
    if(dif == 0) {
      if(atomic_cas_uint8(&ring->get_pos, pos, pos + 1)) {
        break;
      }
    } else if(dif < 0) {
      return 0;
    } else {
      pos = ring->get_pos;
      memory_barrier();
    }
  }
  got_index->i = index;
  got_index->_pos = pos;
  return 1;
}

void
mpmc_ring_get_commit(mpmc_ring_t *ring, const mpmc_ring_index_t *index)
{
  assert(ring != NULL);
  assert(index != NULL);

  ring->sequences[index->i] = index->_pos + ring->mask + 1;
}

int
mpmc_ring_elements(const mpmc_ring_t *ring)
{
  assert(ring != NULL);
  return (ring->put_pos - ring->get_pos) & ring->mask; // TODO. fix this.
}

int
mpmc_ring_empty(const mpmc_ring_t *ring)
{
  return mpmc_ring_elements(ring) == 0;
}

uint8_t
mpmc_ring_size(const mpmc_ring_t *ring)
{
  assert(ring != NULL);
  return ring->mask + 1;
}

//// static inline uint8_t
//// elems(uint8_t put_index, uint8_t get_index, uint8_t mask)
//// {
////   return (put_index - get_index) & mask;
//// }
//// 
//// static inline bool
//// is_empty(uint8_t put_index, uint8_t get_index, uint8_t mask)
//// {
////   return elems(put_index, get_index, mask) == 0;
//// }
//// 
//// static inline bool
//// is_full(uint8_t put_index, uint8_t get_index, uint8_t mask)
//// {
////   return elems(put_index, get_index, mask) == mask;
//// }
//// 
//// static inline uint8_t
//// next(uint8_t i, uint8_t mask)
//// {
////   return (i + 1) & mask;
//// }
//// 
//// /*----------------------------------------------------------------------------------------*/
//// 
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
