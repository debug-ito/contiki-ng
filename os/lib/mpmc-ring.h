#ifndef _MPMC_RING_H_
#define _MPMC_RING_H_

#include "contiki.h"
#include "sys/atomic.h"

/* temporary size */
#define MPMC_RING_SIZE 64
#define MPMC_RING_MASK (MPMC_RING_SIZE - 1)

enum mpmc_ring_state {
  MPMC_RING_EMPTY = 0,
  MPMC_RING_PUTTING,
  MPMC_RING_GETTING,
  MPMC_RING_OCCUPIED,
};

typedef uint8_t mpmc_ring_index_t;

struct mpmc_ring {
  mpmc_ring_index_t put_ptr;
  mpmc_ring_index_t get_ptr;
  uint8_t state[MPMC_RING_SIZE];
};

void mpmc_ring_init(struct mpmc_ring *ring);

int mpmc_ring_put_start(struct mpmc_ring *ring);

int mpmc_ring_put_commit(struct mpmc_ring *ring, mpmc_ring_index_t index);

#endif /* _MPMC_RING_H_ */
