#include <string.h>
#include "lib/assert.h"

#include "mpmc-ring.h"

void
mpmc_ring_init(struct mpmc_ring *ring)
{
  memset(ring, 0, sizeof(*ring));
}

int
mpmc_ring_put_start(struct mpmc_ring *ring)
{
  uint8_t tmp_put;
  assert(ring != NULL);
  for(tmp_put = ring->put_ptr;
      ((tmp_put - ring->get_ptr) & MPMC_RING_MASK) != MPMC_RING_MASK;
      tmp_put = (tmp_put + 1) & MPMC_RING_MASK) {
    if(ring->state[tmp_put] == MPMC_RING_EMPTY &&
       atomic_cas_uint8(&ring->state[tmp_put], MPMC_RING_EMPTY, MPMC_RING_PUTTING)) {
      return tmp_put;
    }
  }
  return -1;
}

int
mpmc_ring_put_commit(struct mpmc_ring *ring, mpmc_ring_index_t index)
{
  assert(ring != NULL);
  assert(ring->state[index] == MPMC_RING_PUTTING);
  
  ring->state[index] = MPMC_RING_OCCUPIED;
  while(ring->state[ring->put_ptr] == MPMC_RING_OCCUPIED) {
    uint8_t now_put = ring->put_ptr;
    atomic_cas_uint8(&ring->put_ptr, now_put, ((now_put + 1) & MPMC_RING_MASK));
  }
  return 1;
}

