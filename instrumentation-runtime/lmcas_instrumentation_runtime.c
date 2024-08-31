// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "lmcas_instrumentation_runtime.h"

// See doc/tabacco/instrumentation-protocol.md for details of the protocol this
// implements the send side of.

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>

static bool lmcas_instrumentation_setup_done = false;

// A hack to remind me that we don't use errno.
#undef errno

// Constants
#define AT_FDCWD -100

/*******************************************************************************
 ******************************* SYSCALL MACROS ********************************
 ******************************************************************************/

// Note that these do not set errno! Also note that these override the
// corresponding functions from musl.

static void lmcas_instrumentation_syscall_start(void);

static long unwrapped_syscall6(long num, long arg1, long arg2, long arg3,
                               long arg4, long arg5, long arg6) {
  long ret;
  register long reg_num __asm__("rax") = (num);
  register long reg_arg1 __asm__("rdi") = (long)(arg1);
  register long reg_arg2 __asm__("rsi") = (long)(arg2);
  register long reg_arg3 __asm__("rdx") = (long)(arg3);
  register long reg_arg4 __asm__("r10") = (long)(arg4);
  register long reg_arg5 __asm__("r8") = (long)(arg5);
  register long reg_arg6 __asm__("r9") = (long)(arg6);

  __asm volatile("syscall\n"
                 : "=a"(ret), "=r"(reg_arg4), "=r"(reg_arg5)
                 : "r"(reg_arg1), "r"(reg_arg2), "r"(reg_arg3), "r"(reg_arg4),
                   "r"(reg_arg5), "r"(reg_arg6), "0"(reg_num)
                 : "rcx", "r11", "memory", "cc");
  return ret;
}

long __syscall6(long num, long arg1, long arg2, long arg3, long arg4, long arg5,
                long arg6) {
  lmcas_instrumentation_syscall_start();

  return unwrapped_syscall6(num, arg1, arg2, arg3, arg4, arg5, arg6);
}

long __syscall5(long num, long arg1, long arg2, long arg3, long arg4,
                long arg5) {
  return __syscall6(num, arg1, arg2, arg3, arg4, arg5, 0);
}

long __syscall4(long num, long arg1, long arg2, long arg3, long arg4) {
  return __syscall6(num, arg1, arg2, arg3, arg4, 0, 0);
}

long __syscall3(long num, long arg1, long arg2, long arg3) {
  return __syscall6(num, arg1, arg2, arg3, 0, 0, 0);
}

long __syscall2(long num, long arg1, long arg2) {
  return __syscall6(num, arg1, arg2, 0, 0, 0, 0);
}

long __syscall1(long num, long arg1) {
  return __syscall6(num, arg1, 0, 0, 0, 0, 0);
}

long __syscall0(long num) { return __syscall6(num, 0, 0, 0, 0, 0, 0); }

long __syscall_cp(long num, long arg1, long arg2, long arg3, long arg4,
                  long arg5, long arg6) {
  return __syscall6(num, arg1, arg2, arg3, arg4, arg5, arg6);
}

#define syscall6(num, arg1, arg2, arg3, arg4, arg5, arg6)                      \
  unwrapped_syscall6(num, (long)(arg1), (long)(arg2), (long)(arg3),            \
                     (long)(arg4), (long)(arg5), (long)(arg6))
#define syscall5(num, arg1, arg2, arg3, arg4, arg5)                            \
  unwrapped_syscall6(num, (long)(arg1), (long)(arg2), (long)(arg3),            \
                     (long)(arg4), (long)(arg5), 0)
#define syscall4(num, arg1, arg2, arg3, arg4)                                  \
  unwrapped_syscall6(num, (long)(arg1), (long)(arg2), (long)(arg3),            \
                     (long)(arg4), 0, 0)
#define syscall3(num, arg1, arg2, arg3)                                        \
  unwrapped_syscall6(num, (long)(arg1), (long)(arg2), (long)(arg3), 0, 0, 0)
#define syscall2(num, arg1, arg2)                                              \
  unwrapped_syscall6(num, (long)(arg1), (long)(arg2), 0, 0, 0, 0)
#define syscall1(num, arg1) unwrapped_syscall6(num, (long)(arg1), 0, 0, 0, 0, 0)
#define syscall0(num) unwrapped_syscall6(num, 0, 0, 0, 0, 0, 0)

#define syscall_cp(num, arg1, arg2, arg3, arg4, arg5, arg6)                    \
  unwrapped_syscall6(num, arg1, arg2, arg3, arg4, arg5, arg6)

/*******************************************************************************
 *********************** UNINSTRUMENTED SYSCALL WRAPPERS ***********************
 ******************************************************************************/

static noreturn void unwrapped_exit(int code) {
  syscall1(__NR_exit, code);
  __builtin_unreachable();
}

static pid_t unwrapped_getpid(void) { return syscall0(__NR_getpid); }

static void *unwrapped_mmap(void *addr, size_t length, int prot, int flags,
                            int fd, off_t offset) {
  return (void *)syscall6(__NR_mmap, addr, length, prot, flags, fd, offset);
}

static ssize_t unwrapped_readlink(const char *restrict path, char *ptr,
                                  size_t len) {
  return syscall3(__NR_readlink, path, ptr, len);
}

static ssize_t unwrapped_write(int fd, const char *ptr, size_t len) {
  return syscall3(__NR_write, fd, ptr, len);
}

/*******************************************************************************
 ************************ HIGHER-LEVEL SYSCALL WRAPPERS ************************
 ******************************************************************************/

static void write_all_or_die(int fd, const char *ptr, size_t len);

static void write_str_or_die(int fd, const char *str) {
  write_all_or_die(fd, str, strlen(str));
}

static noreturn void die(const char *msg, int err) {
  write_str_or_die(2, "lmcas_instrumentation_runtime: ");
  write_str_or_die(2, msg);
  write_str_or_die(2, ": ");
  write_str_or_die(2, strerror(err));
  write_str_or_die(2, "\n");
  unwrapped_exit(-err);
}

static void write_all_or_die(int fd, const char *ptr, size_t len) {
  while (len) {
    ssize_t ret = unwrapped_write(fd, ptr, len);
    if (ret == -EINTR) {
      // Just rerun the syscall.
    } else if (ret < 0) {
      // If we weren't writing to stderr, complain to it; either way, die. We
      // don't use the die() helper here, because we want to ensure that fd
      // isn't 2 before we do the printing.
      if (fd != 2) {
        write_str_or_die(2, "lmcas_instrumentation_runtime: write() failed: ");
        write_str_or_die(2, strerror(-ret));
        write_str_or_die(2, "\n");
      }
      unwrapped_exit(-ret);
    } else {
      ptr += ret;
      len -= ret;
    }
  }
}

/*******************************************************************************
 *********************************** HELPERS ***********************************
 ******************************************************************************/

// Returns whether the bytestring given by ptr and len matches the regex
// `pipe:\[[0-9]+\]`.
static bool check_pipe(const char *ptr, size_t len) {
  if (len < 8) // len("pipe:[") + len("]") + at least one 0-9 digit
    return false;
  if (memcmp(ptr, "pipe:[", 6) != 0)
    return false;
  ptr += 6;
  len -= 6;

  while (len-- != 1) {
    char ch = *ptr++;
    if (!('0' <= ch && ch <= '9'))
      return false;
  }

  if (*ptr != ']')
    return false;

  return true;
}

/*******************************************************************************
 ***************************** INSTRUMENTATION API *****************************
 ******************************************************************************/

struct lmcas_function_pointer_entry {
  uintptr_t ptr;
  uint64_t id;
};

extern const struct lmcas_function_pointer_entry
    __start_lmcas_function_pointer_table;
// Never dereference this!
extern const struct lmcas_function_pointer_entry
    __stop_lmcas_function_pointer_table;

void _lmcas_noop(int signo, siginfo_t *info, void *context);

void lmcas_instrumentation_setup(void) {
  // First, check that fd 1023 is a pipe; the pipe is created by the
  // instrumentation-parent process.
  char fd3_path[64];
  ssize_t fd3_path_len =
      unwrapped_readlink("/proc/self/fd/1023", &fd3_path[0], 64);
  if (fd3_path_len < 0)
    die("lmcas_instrumentation_runtime: could not "
        "readlink(\"/proc/self/fd/1023\")",
        -fd3_path_len);
  if (!check_pipe(fd3_path, fd3_path_len)) {
    write_str_or_die(2, "lmcas_instrumentation_runtime: "
                        "readlink(\"/proc/self/fd/1023\", ...) "
                        "was not a pipe, but instead ");
    write_all_or_die(2, fd3_path, fd3_path_len);
    write_str_or_die(2, "\n");
    unwrapped_exit(1);
  }

  // Allocate a page for the instrumentation-parent process to write data into.
  // This is used to mock syscalls.
  void *parent_page = unwrapped_mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  uint64_t parent_page_addr = (uint64_t)parent_page;
  if (parent_page_addr > (uint64_t)-4096) {
    die("lmcas_instrumentation_runtime: could not mmap(NULL, 4096, "
        "PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)",
        -parent_page_addr);
  }

  // Count the number of function pointers to send the parent.
  struct lmcas_function_pointer_entry const *function_pointer_start =
      &__start_lmcas_function_pointer_table;
  size_t function_pointer_count = &__stop_lmcas_function_pointer_table -
                                  &__start_lmcas_function_pointer_table;

  // Allocate space for the message.
  size_t msg_size = 37;
  char *msg = mmap(NULL, msg_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  // Create the message.
  msg[0] = 'R';
  pid_t pid = unwrapped_getpid();
  memcpy(&msg[1], &pid, 4);
  memcpy(&msg[5], &parent_page_addr, 8);
  void (*noop)(int signo, siginfo_t *info, void *context) = _lmcas_noop;
  memcpy(&msg[13], &noop, 8);
  memcpy(&msg[21], &function_pointer_start, 8);
  memcpy(&msg[29], &function_pointer_count, 8);

  // Send the `R` message. After this message is processed, all syscalls are
  // monitored by the instrumentation-parent process.
  write_all_or_die(1023, msg, msg_size);

  // Mark the setup as complete.
  lmcas_instrumentation_setup_done = true;
}

noreturn void lmcas_instrumentation_done(void) {
  if (!lmcas_instrumentation_setup_done) {
    write_str_or_die(2, "lmcas_instrumentation_runtime: "
                        "lmcas_instrumentation_done was called, but "
                        "lmcas_instrumentation_setup never was!\n");
    unwrapped_exit(1);
  }

  char msg[1];
  msg[0] = 'D';
  write_all_or_die(1023, msg, 1);
  // The instrumentation-parent process should kill us before execution
  // continues here.
  abort();
}

void lmcas_instrumentation_bb_start(uint64_t bb_id) {
  if (!lmcas_instrumentation_setup_done)
    return;

  char msg[9];
  msg[0] = 'B';
  memcpy(&msg[1], &bb_id, 8);
  write_all_or_die(1023, msg, 9);
}

void lmcas_instrumentation_call_start(void) {
  if (!lmcas_instrumentation_setup_done)
    return;

  char msg[2];
  msg[0] = 'C';
  msg[1] = 's';
  write_all_or_die(1023, msg, 2);
}

void lmcas_instrumentation_call_end(void) {
  if (!lmcas_instrumentation_setup_done)
    return;

  char msg[2];
  msg[0] = 'C';
  msg[1] = 'e';
  write_all_or_die(1023, msg, 2);
}

static void lmcas_instrumentation_syscall_start(void) {
  if (!lmcas_instrumentation_setup_done)
    return;

  char msg[1];
  msg[0] = 'S';
  write_all_or_die(1023, msg, 1);
}

// At each ret terminator, this function is called to record it.
void lmcas_instrumentation_record_ret(void) {
  if (!lmcas_instrumentation_setup_done)
    return;

  char msg[1];
  msg[0] = 'r';
  write_all_or_die(1023, msg, 1);
}

void lmcas_instrumentation_record_cond_br(uint8_t cond) {
  if (!lmcas_instrumentation_setup_done)
    return;

  char msg[2];
  msg[0] = 'c';
  msg[1] = !!cond;
  write_all_or_die(1023, msg, 2);
}

// There's no record function for an unconditional br, since we don't need to
// record that.

// At each switch terminator, this function is called to record the destination.
// The value being switched on should be passed here as an i64.
void lmcas_instrumentation_record_switch(uint64_t value) {
  if (!lmcas_instrumentation_setup_done)
    return;

  char msg[9];
  msg[0] = 's';
  memcpy(&msg[1], &value, 8);
  write_all_or_die(1023, msg, 9);
}

// At each indirectbr terminator, this function is called to record the
// destination. The address being branched to should be passed here as the
// target's pointer size.
void lmcas_instrumentation_record_indirectbr(uintptr_t addr) {
  if (!lmcas_instrumentation_setup_done)
    return;

  char msg[9];
  msg[0] = 'i';
  memcpy(&msg[1], &addr, 8);
  write_all_or_die(1023, msg, 9);
}

// At the unreachable terminator, this function is called to record it. This,
// of course, shouldn't be called either.
void lmcas_instrumentation_record_unreachable(void) {
  if (!lmcas_instrumentation_setup_done)
    return;

  char msg[1];
  msg[0] = 'u';
  write_all_or_die(1023, msg, 1);
}
