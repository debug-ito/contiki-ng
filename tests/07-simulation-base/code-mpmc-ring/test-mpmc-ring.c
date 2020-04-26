/*
 * Copyright (c) 2020, Toshio Ito
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
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

#include <stdio.h>

#include "contiki.h"
#include "unit-test/unit-test.h"

#include "lib/mpmc-ring.h"

PROCESS(test_process, "mpmc-ring test");
AUTOSTART_PROCESSES(&test_process);

MPMC_RING(ring2, 2);
MPMC_RING(ring32, 32);
MPMC_RING(ring128, 128);

static void
init_index(mpmc_ring_index_t *i)
{
  i->i = 0;
  i->_pos = 0;
}

void
test_print_report(const unit_test_t *utp)
{
  printf("=check-me= ");
  if(utp->result == unit_test_failure) {
    printf("FAILED   - %s: exit at L%u\n", utp->descr, utp->exit_line);
  } else {
    printf("SUCCEEDED - %s\n", utp->descr);
  }
}

UNIT_TEST_REGISTER(test_init_get, "Init and get");
UNIT_TEST(test_init_get)
{
  mpmc_ring_index_t index;
  int is_success;
  
  UNIT_TEST_BEGIN();

  mpmc_ring_init(&ring32);
  UNIT_TEST_ASSERT(mpmc_ring_elements(&ring32) == 0);
  UNIT_TEST_ASSERT(mpmc_ring_empty(&ring32));
  is_success = mpmc_ring_get_begin(&ring32, &index);
  UNIT_TEST_ASSERT(!is_success);

  UNIT_TEST_END();
}

UNIT_TEST_REGISTER(test_put_get, "Put and get");
UNIT_TEST(test_put_get)
{
  const int LOOP_NUM = 50;
  mpmc_ring_index_t index;
  int vals[32];
  int put_val = 100;
  int got_val;
  int i;
  int is_success;

  UNIT_TEST_BEGIN();

  mpmc_ring_init(&ring32);
  for(i = 0 ; i < LOOP_NUM ; i++) {
    init_index(&index);
    is_success = mpmc_ring_put_begin(&ring32, &index);
    UNIT_TEST_ASSERT(is_success);
    vals[index.i] = put_val;
    mpmc_ring_put_commit(&ring32, &index);

    UNIT_TEST_ASSERT(mpmc_ring_elements(&ring32) == 1);

    init_index(&index);
    is_success = mpmc_ring_get_begin(&ring32, &index);
    UNIT_TEST_ASSERT(is_success);
    got_val = vals[index.i];
    mpmc_ring_get_commit(&ring32, &index);

    UNIT_TEST_ASSERT(got_val == put_val);
    put_val++;
    
    UNIT_TEST_ASSERT(mpmc_ring_elements(&ring32) == 0);
  }
  
  UNIT_TEST_END();
}

UNIT_TEST_REGISTER(test_drain255, "Drain at pos 255");
UNIT_TEST(test_drain255)
{
  mpmc_ring_index_t index;
  int i;
  int is_success;
  int vals[32];
  int got;
  
  UNIT_TEST_BEGIN();

  mpmc_ring_init(&ring32);

  for(i = 0 ; i < 255 ; i++) {
    is_success = mpmc_ring_put_begin(&ring32, &index);
    UNIT_TEST_ASSERT(is_success);
    vals[index.i] = 77 + i;
    mpmc_ring_put_commit(&ring32, &index);

    UNIT_TEST_ASSERT(mpmc_ring_elements(&ring32) == 1);

    is_success = mpmc_ring_get_begin(&ring32, &index);
    UNIT_TEST_ASSERT(is_success);
    got = vals[index.i];
    mpmc_ring_get_commit(&ring32, &index);
    UNIT_TEST_ASSERT(got == 77 + i);
    UNIT_TEST_ASSERT(mpmc_ring_elements(&ring32) == 0);
  }

  // this should return failure immediately (without blocking)
  is_success = mpmc_ring_get_begin(&ring32, &index);
  UNIT_TEST_ASSERT(!is_success);
  
  UNIT_TEST_END();
}

UNIT_TEST_REGISTER(test_full_at_wrapped0, "Full at wrapped pos 0");
UNIT_TEST(test_full_at_wrapped0)
{
  mpmc_ring_index_t index;
  int i;
  int is_success;
  int vals[2];
  int got;
  
  UNIT_TEST_BEGIN();

  mpmc_ring_init(&ring2);

  for(i = 0 ; i < 254 ; i++) {
    is_success = mpmc_ring_put_begin(&ring2, &index);
    UNIT_TEST_ASSERT(is_success);
    vals[index.i] = 77 + i;
    mpmc_ring_put_commit(&ring2, &index);

    UNIT_TEST_ASSERT(mpmc_ring_elements(&ring2) == 1);

    is_success = mpmc_ring_get_begin(&ring2, &index);
    UNIT_TEST_ASSERT(is_success);
    got = vals[index.i];
    mpmc_ring_get_commit(&ring2, &index);
    UNIT_TEST_ASSERT(got == 77 + i);
    UNIT_TEST_ASSERT(mpmc_ring_elements(&ring2) == 0);
  }

  is_success = mpmc_ring_put_begin(&ring2, &index);
  UNIT_TEST_ASSERT(is_success);
  vals[index.i] = 888;
  mpmc_ring_put_commit(&ring2, &index);

  UNIT_TEST_ASSERT(mpmc_ring_elements(&ring2) == 1);

  is_success = mpmc_ring_put_begin(&ring2, &index);
  UNIT_TEST_ASSERT(is_success);
  vals[index.i] = 889;
  mpmc_ring_put_commit(&ring2, &index);

  UNIT_TEST_ASSERT(mpmc_ring_elements(&ring2) == 2);

  // this should return failure immediately (without blocking)
  is_success = mpmc_ring_put_begin(&ring2, &index);
  UNIT_TEST_ASSERT(!is_success);

  UNIT_TEST_ASSERT(mpmc_ring_elements(&ring2) == 2);
  
  UNIT_TEST_END();
}

UNIT_TEST_REGISTER(test_queue128, "Queue of length 128");
UNIT_TEST(test_queue128)
{
  uint16_t vals[128];
  uint16_t put_val = 231;
  uint16_t exp_get_val = put_val;
  int is_success;
  int i;
  mpmc_ring_index_t index;
  
  UNIT_TEST_BEGIN();

  mpmc_ring_init(&ring128);
  for(i = 0 ; i < 128 ; i++) {
    is_success = mpmc_ring_put_begin(&ring128, &index);
    UNIT_TEST_ASSERT(is_success);
    vals[index.i] = put_val;
    put_val++;
    mpmc_ring_put_commit(&ring128, &index);
  }
  UNIT_TEST_ASSERT(mpmc_ring_elements(&ring128) == 128);
  is_success = mpmc_ring_put_begin(&ring128, &index);
  UNIT_TEST_ASSERT(!is_success);

  for(i = 0 ; i < 32 ; i++) {
    is_success = mpmc_ring_get_begin(&ring128, &index);
    UNIT_TEST_ASSERT(is_success);
    UNIT_TEST_ASSERT(vals[index.i] == exp_get_val);
    mpmc_ring_get_commit(&ring128, &index);
    exp_get_val++;
  }
  UNIT_TEST_ASSERT(mpmc_ring_elements(&ring128) == 96);

  for(i = 0 ; i < 256 ; i++) {
    is_success = mpmc_ring_put_begin(&ring128, &index);
    UNIT_TEST_ASSERT(is_success);
    vals[index.i] = put_val;
    put_val++;
    mpmc_ring_put_commit(&ring128, &index);
    UNIT_TEST_ASSERT(mpmc_ring_elements(&ring128) == 97);

    is_success = mpmc_ring_get_begin(&ring128, &index);
    UNIT_TEST_ASSERT(is_success);
    UNIT_TEST_ASSERT(vals[index.i] == exp_get_val);
    mpmc_ring_get_commit(&ring128, &index);
    exp_get_val++;
    UNIT_TEST_ASSERT(mpmc_ring_elements(&ring128) == 96);
  }

  for(i = 0 ; i < 96 ; i++) {
    is_success = mpmc_ring_get_begin(&ring128, &index);
    UNIT_TEST_ASSERT(is_success);
    UNIT_TEST_ASSERT(vals[index.i] == exp_get_val);
    mpmc_ring_get_commit(&ring128, &index);
    exp_get_val++;
  }
  UNIT_TEST_ASSERT(mpmc_ring_elements(&ring128) == 0);
  is_success = mpmc_ring_get_begin(&ring128, &index);
  UNIT_TEST_ASSERT(!is_success);
  
  UNIT_TEST_END();
}

PROCESS_THREAD(test_process, ev, data)
{
  PROCESS_BEGIN();
  printf("Run unit-test\n");
  printf("---\n");

  UNIT_TEST_RUN(test_init_get);
  UNIT_TEST_RUN(test_put_get);
  UNIT_TEST_RUN(test_drain255);
  UNIT_TEST_RUN(test_full_at_wrapped0);
  UNIT_TEST_RUN(test_queue128);

  printf("=check-me= DONE\n");
  PROCESS_END();
}
