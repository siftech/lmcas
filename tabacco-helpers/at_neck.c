#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

// Weakness ensures that there's no linker error if the tabacco_pre_neck section
// doesn't exist.
__attribute__((weak)) extern char __start_tabacco_pre_neck;
__attribute__((weak)) extern char __stop_tabacco_pre_neck;

static void fill_with_int3(void *start, void *end) {
  if (start == end)
    return;

  // Unprotect the pages around the region.
  void *pages_start = (void *)((uintptr_t)start & ~4095);
  void *pages_end = (void *)(((uintptr_t)end + 4095) & ~4095);
  if (mprotect(pages_start, pages_end - pages_start,
               PROT_READ | PROT_WRITE | PROT_EXEC)) {
    perror("tabacco: mprotect");
    abort();
  }

  // Set the region to the INT3 byte.
  memset(start, 0xcc, end - start);

  // Reprotect them to r-x.
  if (mprotect(pages_start, pages_end - pages_start, PROT_READ | PROT_EXEC)) {
    perror("tabacco: mprotect");
    abort();
  }
}

static void must_unmap(void *start, void *end) {
  if (munmap(start, end - start)) {
    perror("tabacco: munmap");
    abort();
  }
}

void _tabacco_at_neck(void) {
  void *start = &__start_tabacco_pre_neck, *end = &__stop_tabacco_pre_neck;

  void *first_page = (void *)(((uintptr_t)start + 4095) & ~4095);
  void *last_page = (void *)((uintptr_t)end & ~4095);

  if (start <= first_page && first_page < last_page && last_page <= end) {
    // If there are entire pages to unmap, do that, then unmap the beginning and
    // end.
    fill_with_int3(start, first_page);
    must_unmap(first_page, last_page);
    fill_with_int3(last_page, end);
  } else {
    // Otherwise, something weird is going on, so just write out the INT3s.
    fill_with_int3(start, end);
  }
}
