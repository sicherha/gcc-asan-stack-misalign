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

#define BLAKE3_OUT_LEN 32
#define BLAKE3_BLOCK_LEN 64
#define BLAKE3_CHUNK_LEN 1024

typedef struct {
  unsigned cv[8];
  unsigned long chunk_counter;
  unsigned char buf[BLAKE3_BLOCK_LEN];
} blake3_chunk_state;

#define MAX_SIMD_DEGREE 8

typedef struct {
  unsigned char block[BLAKE3_BLOCK_LEN];
} output_t;

static inline output_t
make_output(const unsigned char block[BLAKE3_BLOCK_LEN]) {
  output_t ret;
  __builtin_memcpy(ret.block, block, BLAKE3_BLOCK_LEN);
  return ret;
}

static void blake3_compress_subtree_wide(unsigned long input_len,
                                         unsigned char *out) {
  if (input_len <= BLAKE3_CHUNK_LEN) {
    unsigned long input_position = 0;
    while (input_len - input_position >= BLAKE3_CHUNK_LEN) {
      input_position += BLAKE3_CHUNK_LEN;
    }

    blake3_chunk_state chunk_state = {};
    make_output(chunk_state.buf);
    return;
  }

  unsigned char cv_array[2 * MAX_SIMD_DEGREE * BLAKE3_OUT_LEN];
  blake3_compress_subtree_wide(input_len / 2, cv_array);

  // SIGSEGV triggered in the next line by an unaligned `vmovdqa64` load
  __builtin_memcpy(out, cv_array, 2 * BLAKE3_OUT_LEN);
}

int main(void) {
  unsigned char cv_array[MAX_SIMD_DEGREE * BLAKE3_OUT_LEN];
  blake3_compress_subtree_wide(2048, cv_array);

  return 0;
}
