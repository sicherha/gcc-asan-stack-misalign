# Stack-misalignment bug in GCC's AddressSanitizer

https://gcc.gnu.org/bugzilla/show_bug.cgi?id=110027

Under certain conditions, GCC's AddressSanitizer implementation generates
accesses to memory addresses that are not suitably aligned for AVX-512,
resulting in a segmentation fault.

The core of the issue is the placement of variables on a fake stack by
AddressSanitizer's
[stack-use-after-return](https://github.com/google/sanitizers/wiki/AddressSanitizerUseAfterReturn)
check.

## Reproducing the bug

Prerequisites:

* An x86_64 CPU
  [supporting AVX-512](https://en.wikipedia.org/wiki/AVX-512#CPUs_with_AVX-512)
* GCC >= 12
* libasan
* Make

[repro.c](repro.c) contains a minimal program that reproduces the error.
Build and run it by typing:

```sh
make run
```
The binary will be executed with the AddressSanitizer option
`detect_stack_use_after_return=1` and end up crashing with a segmentation fault.

By contrast, when setting `detect_stack_use_after_return=0`, the binary will run
just fine:

```sh
make run-nofail
```

## Analysis

GCC compiles the `__builtin_memcpy(out, cv_array, 2 * BLAKE3_OUT_LEN);` call at
the end of `blake3_compress_subtree_wide()` into a `vmovdqa64`/`vmovdqu64` pair.
The`vmovdqa64` instruction requires its memory operand to be aligned on a
64-byte boundary, whereas `vmovdqu64` does not demand a particular alignment.

The load part looks as follows:

```asm
vmovdqa64 -0x240(%rbx),%zmm0
```

With AddressSanitizer's *stack-use-after-return* check **disabled** at runtime
(by setting `ASAN_OPTIONS=detect_stack_use_after_return=0` in the environment),
`-0x240(%rbx)` is a valid address that points into the stack frame and is a
multiple of 64 bytes.

With the *stack-use-after-return* check **enabled**
(`ASAN_OPTIONS=detect_stack_use_after_return=1`), an additional fake stack frame
is allocated by libasan. `-0x240(%rbx)` then points into that fake stack; it is
divisible by 32 but not by 64. Executing this instruction triggers a
segmentation fault.

## Discussion

Since GCC has full control over the placement of `cv_array` in the regular stack
frame, it is within its rights to choose a 64-byte alignment and emit a
`vmovdqa64` instruction.

However, GCC does not appear to properly account for the fact that libasan might
place `cv_array` on a fake stack. The fake stack itself is aligned at a page
boundary (4096 bytes), but the offset of `cv_array` within that page is
misaligned.

GCC should either enforce the desired alignment of variables placed on the fake
stack, or it should weaken its assumptions regarding the alignment of such
variables.
