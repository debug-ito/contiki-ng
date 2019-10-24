#ifndef _MPMC_RING_H_
#define _MPMC_RING_H_

#include "contiki.h"
#include "sys/atomic.h"
#include "sys/cc.h"

typedef uint8_t mpmc_ring_index_t;

struct mpmc_ring {
  mpmc_ring_index_t put_ptr;
  mpmc_ring_index_t get_ptr;
  uint8_t * const state;
  const mpmc_ring_index_t mask;
};

/**
 * \param num Must be power of 2.
 */
#define MPMC_RING(name, size)                                           \
  static uint8_t CC_CONCAT(name,_state)[size];                          \
  static struct mpmc_ring name = { 0, 0, CC_CONCAT(name,_state), (size) - 1 };

void mpmc_ring_init(struct mpmc_ring *ring);

int mpmc_ring_put_start(struct mpmc_ring *ring);

int mpmc_ring_put_commit(struct mpmc_ring *ring, mpmc_ring_index_t index);

int mpmc_ring_get_start(struct mpmc_ring *ring);

int mpmc_ring_get_commit(struct mpmc_ring *ring, mpmc_ring_index_t index);

int mpmc_ring_elements(const struct mpmc_ring *ring);

int mpmc_ring_empty(const struct mpmc_ring *ring);

#endif /* _MPMC_RING_H_ */
