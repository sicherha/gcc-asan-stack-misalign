// Program reproducing an AVX-512 misalignment bug in GCC's AddressSanitizer
//
// This program does not do anything useful. The code has been stripped down
// almost beyond recognition from the C reference implementation of the BLAKE3
// cryptographic-hash algorithm (https://github.com/BLAKE3-team/BLAKE3), where
// the bug was initially observed.
//
// This is the simplest version of the code that has been found to reproducibly
// trigger the bug across multiple distributions and optimisation levels.
// Further attempts at boiling it down have resulted in things becoming flaky.

#include <stdint.h>
#include <string.h>

#define BLAKE3_OUT_LEN 32
#define BLAKE3_BLOCK_LEN 64
#define BLAKE3_CHUNK_LEN 1024

typedef struct {
  uint32_t cv[8];
  uint64_t chunk_counter;
  uint8_t buf[BLAKE3_BLOCK_LEN];
} blake3_chunk_state;

#define MAX_SIMD_DEGREE 8

typedef struct {
  uint32_t input_cv[8];
  uint64_t counter;
  uint8_t block[BLAKE3_BLOCK_LEN];
} output_t;

static inline output_t make_output(const uint32_t input_cv[8],
                                   const uint8_t block[BLAKE3_BLOCK_LEN],
                                   uint64_t counter) {
  output_t ret;
  memcpy(ret.input_cv, input_cv, 32);
  memcpy(ret.block, block, BLAKE3_BLOCK_LEN);
  ret.counter = counter;
  return ret;
}

static size_t blake3_compress_subtree_wide(size_t input_len,
                                           uint8_t *out) {
  if (input_len <= BLAKE3_CHUNK_LEN) {
    size_t input_position = 0;
    size_t chunks_array_len = 0;
    while (input_len - input_position >= BLAKE3_CHUNK_LEN) {
      input_position += BLAKE3_CHUNK_LEN;
      chunks_array_len += 1;
    }

    if (input_len > input_position) {
      blake3_chunk_state chunk_state = {};
      output_t output = make_output(chunk_state.cv,
                                    chunk_state.buf,
                                    chunk_state.chunk_counter);
      memcpy(&out[chunks_array_len * BLAKE3_OUT_LEN],
             output.input_cv,
             sizeof(output.input_cv));
      return chunks_array_len + 1;
    } else {
      memset(out, 0, 2 * MAX_SIMD_DEGREE * BLAKE3_OUT_LEN);
      return chunks_array_len;
    }
  }

  uint8_t cv_array[2 * MAX_SIMD_DEGREE * BLAKE3_OUT_LEN];
  blake3_compress_subtree_wide(input_len / 2, cv_array);

  // SIGSEGV triggered in the next line by an unaligned `vmovdqa64` load
  memcpy(out, cv_array, 2 * BLAKE3_OUT_LEN);

  return 2;
}

int main(void) {
  uint8_t cv_array[MAX_SIMD_DEGREE * BLAKE3_OUT_LEN];
  blake3_compress_subtree_wide(2048, cv_array);

  return 0;
}
