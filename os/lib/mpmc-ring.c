#include <string.h>
#include "lib/assert.h"

#include "mpmc-ring.h"

/*----------------------------------------------------------------------------------------*/

enum mpmc_ring_state {
  MPMC_RING_EMPTY = 0,
  MPMC_RING_PUTTING,
  MPMC_RING_GETTING,
  MPMC_RING_OCCUPIED,
};

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

void
mpmc_ring_init(struct mpmc_ring *ring)
{
  ring->put_ptr = 0;
  ring->get_ptr = 0;
  memset(ring->state, 0, (size_t)ring->mask + 1);
}

int
mpmc_ring_put_start(struct mpmc_ring *ring)
{
  mpmc_ring_index_t tmp_put;
  mpmc_ring_index_t now_get;
  assert(ring != NULL);
  now_get = ring->get_ptr;
  for(tmp_put = ring->put_ptr;
      !is_full(tmp_put, now_get, ring->mask);
      tmp_put = next(tmp_put, ring->mask)) {
    if(ring->state[tmp_put] == MPMC_RING_EMPTY &&
       atomic_cas_uint8(&ring->state[tmp_put], MPMC_RING_EMPTY, MPMC_RING_PUTTING)) {
      return tmp_put;
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
  now_get = ring->get_ptr;
  while(ring->state[ring->put_ptr] == MPMC_RING_OCCUPIED
        && !is_full(ring->put_ptr, now_get, ring->mask)) {
    mpmc_ring_index_t now_put = ring->put_ptr;
    atomic_cas_uint8(&ring->put_ptr, now_put, next(now_put, ring->mask));
  }
}

int
mpmc_ring_get_start(struct mpmc_ring *ring)
{
  mpmc_ring_index_t tmp_get;
  mpmc_ring_index_t now_put;
  assert(ring != NULL);
  now_put = ring->put_ptr;
  for(tmp_get = ring->get_ptr;
      !is_empty(now_put, tmp_get, ring->mask);
      tmp_get = next(tmp_get, ring->mask)) {
    if(ring->state[tmp_get] == MPMC_RING_OCCUPIED &&
       atomic_cas_uint8(&ring->state[tmp_get], MPMC_RING_OCCUPIED, MPMC_RING_GETTING)) {
      return tmp_get;
    }
  }
  return -1;
}

void
mpmc_ring_get_commit(struct mpmc_ring *ring, mpmc_ring_index_t index)
{
  mpmc_ring_index_t now_put;
  assert(ring != NULL);
  assert(ring->state[index] == MPMC_RING_GETTING);
  
  ring->state[index] = MPMC_RING_EMPTY;
  now_put = ring->put_ptr;
  while(ring->state[ring->get_ptr] == MPMC_RING_EMPTY
        && !is_empty(now_put, ring->get_ptr, ring->mask)) {
    mpmc_ring_index_t now_get = ring->get_ptr;
    atomic_cas_uint8(&ring->get_ptr, now_get, next(now_get, ring->mask));
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
