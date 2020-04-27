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

  assert(ring->mask < 64);
  assert(ring->mask > 0);
  assert((ring->mask & (ring->mask + 1)) == 0);
  
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
  int8_t dif = ((int8_t)ring->put_pos - (int8_t)ring->get_pos);
  return dif;

  /*
   * The code below is necessary if we allow mpmc_ring of size
   * 128. The else clause is especially for the case where size == 128
   * && elements = 128.
   *
   * if(dif >= 0) {
   *   return (int)dif;
   * } else {
   *   int ret = (int)dif;
   *   while(ret <= 0) {
   *     ret += ring->mask + 1;
   *   }
   *   return ret;
   * }
   */
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

