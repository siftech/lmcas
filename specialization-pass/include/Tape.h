// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef TABACCO__TAPE_H
#define TABACCO__TAPE_H

#include <cstdint>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <variant>
#include <vector>

namespace tape {

// Because of JSON's origins in the JavaScript world, many implementations
// treat all numbers as doubles, and in particular, round numbers to fit in the
// range that doubles can represent. Since we need to exactly represent numbers,
// we do the common trick of encoding them as strings instead.
//
// The i64_as_string and u64_as_string types are thin wrappers around an
// int64_t and uint64_t, respectively, that get deserialized as strings.
struct i64_as_string {
  int64_t value;

  i64_as_string() = default;
  i64_as_string(const int64_t v) : value(v) {}
  i64_as_string(const i64_as_string &v) = default;
  i64_as_string &operator=(const i64_as_string &rhs) = default;
  i64_as_string &operator=(const int64_t &rhs) {
    value = rhs;
    return *this;
  }
  operator const int64_t &() const { return value; }
  bool operator==(const i64_as_string &rhs) const { return value == rhs.value; }
  bool operator==(const int64_t &rhs) const { return value == rhs; }
};

struct u64_as_string {
  uint64_t value;

  u64_as_string() = default;
  u64_as_string(const uint64_t v) : value(v) {}
  u64_as_string(const u64_as_string &v) = default;
  u64_as_string &operator=(const u64_as_string &rhs) = default;
  u64_as_string &operator=(const uint64_t &rhs) {
    value = rhs;
    return *this;
  }
  operator const uint64_t &() const { return value; }
  bool operator==(const u64_as_string &rhs) const { return value == rhs.value; }
  bool operator==(const uint64_t &rhs) const { return value == rhs; }
};

struct string_as_array {
  std::string value;
  string_as_array() = default;
  string_as_array(const std::string v) : value(v) {}
  string_as_array(const string_as_array &v) = default;
  string_as_array &operator=(const string_as_array &rhs) = default;
  string_as_array &operator=(const std::string &rhs) {
    value = rhs;
    return *this;
  }
  operator const std::string &() const { return value; }
  bool operator==(const string_as_array &rhs) const {
    return value == rhs.value;
  }
  bool operator==(const std::string &rhs) const { return value == rhs; }
};

// Each variant of the TapeEntry enum from instrumentation-parent gets its own
// struct.

struct BasicBlockStart {
  u64_as_string basic_block_id;
};

struct CallInfo {
  bool start;
};

struct IOVecForSyscall {
  u64_as_string base;
  u64_as_string len;
  std::vector<uint8_t> data;
};

struct SyscallRead {
  int32_t fd;
  u64_as_string count;
  std::vector<uint8_t> data;
  u64_as_string return_code;
};

struct SyscallWrite {
  int32_t fd;
  std::vector<uint8_t> data;
  u64_as_string return_code;
};

struct SyscallOpen {
  std::string filename;
  uint32_t flags;
  uint32_t mode;
  u64_as_string return_code;
};

struct SyscallStat {
  struct stat data;
  std::string filename;
  u64_as_string return_code;
};

struct SyscallFstat {
  int32_t fd;

  u64_as_string return_code;
  struct stat data;
};

struct SyscallClose {
  int32_t fd;

  u64_as_string return_code;
};

struct SyscallLseek {
  int32_t fd;
  i64_as_string offset;
  uint32_t origin;

  u64_as_string return_code;
};

struct SyscallMmap {
  u64_as_string addr;
  u64_as_string len;
  int32_t prot;
  int32_t flags;
  int32_t fd;
  i64_as_string off;

  u64_as_string return_code;
};

struct SyscallMprotect {
  u64_as_string start;
  u64_as_string len;
  int32_t prot;

  u64_as_string return_code;
};

struct SyscallMunmap {
  u64_as_string addr;
  u64_as_string len;

  u64_as_string return_code;
};

struct SyscallBrk {
  u64_as_string brk;

  u64_as_string return_code;
};

struct Sigset {
  std::array<u64_as_string, 1> val;
};

struct Sigaction {
  u64_as_string sa_handler_;
  u64_as_string sa_flags;
  u64_as_string sa_restorer;
  Sigset sa_mask;
};

struct SyscallRtSigaction {
  u64_as_string return_code;
  int32_t sig;
  std::optional<Sigaction> act;
  std::optional<Sigaction> oact;
  u64_as_string sigsetsize;
  u64_as_string sighandler;
};

struct SyscallRtSigprocmask {
  u64_as_string return_code;
  int32_t how;
  u64_as_string sigsetsize;
  std::optional<Sigset> oset;
  std::optional<Sigset> nset;
};

struct SyscallIoctl {
  int32_t fd;
  u64_as_string request;
  u64_as_string arg0;
  u64_as_string arg1;
  u64_as_string arg2;
  u64_as_string arg3;
  std::optional<int32_t> arg0_contents;

  u64_as_string return_code;
};

struct SyscallPread {
  int32_t fd;
  u64_as_string count;
  i64_as_string pos;
  std::vector<uint8_t> data;
  u64_as_string return_code;
};

struct SyscallReadv {
  int32_t fd;
  u64_as_string iov;
  u64_as_string iovcnt;

  u64_as_string return_code;
  std::vector<IOVecForSyscall> iovs;
};

struct SyscallWritev {
  int32_t fd;
  u64_as_string iov;
  u64_as_string iovcnt;

  u64_as_string return_code;
  std::vector<IOVecForSyscall> iovs;
};

struct SyscallPipe {
  u64_as_string return_code;
  std::vector<int32_t> pipefds;
};

struct SyscallSocket {
  u64_as_string return_code;
  int32_t family;
  int32_t type_socket;
  int32_t protocol;
};

struct SyscallConnect {
  u64_as_string return_code;
  int32_t fd;
  std::vector<uint8_t> sockaddr_data;
  u64_as_string addrlen;
};

struct SyscallBind {
  u64_as_string return_code;
  int32_t fd;
  std::vector<uint8_t> sockaddr_data;
  u64_as_string addrlen;
};

struct SyscallListen {
  int32_t fd;
  int32_t backlog;
  u64_as_string return_code;
};

struct SyscallSetsockopt {
  int32_t fd;
  int32_t level;
  int32_t optname;
  int32_t optlen;
  std::string optval;
  u64_as_string return_code;
};

struct UTSName {
  std::string sysname;    /* Operating system name (e.g., "Linux") */
  std::string nodename;   /* Name within "some implementation-defined
                       network" */
  std::string release;    /* Operating system release
                        (e.g., "2.6.28") */
  std::string version;    /* Operating system version */
  std::string machine;    /* Hardware identifier */
  std::string domainname; /* NIS or YP domain name */
};

struct SyscallUname {
  u64_as_string return_code;
  std::vector<uint8_t> data;
};

struct SyscallOpenat {
  // TODO
};

struct SyscallFcntl {
  int32_t fd;
  uint32_t command;
  u64_as_string arg;
  u64_as_string return_code;
};

struct SyscallMkdir {
  std::string pathname;
  uint32_t mode;
  u64_as_string return_code;
};

struct SyscallUmask {
  u64_as_string return_code;
  // uint32_t return_code;
};

struct SyscallGetuid {
  u64_as_string return_code;
};

struct SyscallGetpid {
  u64_as_string return_code;
};

struct SyscallGetppid {
  u64_as_string return_code;
};

struct SyscallGeteuid {
  u64_as_string return_code;
};

struct SyscallGetgid {
  u64_as_string return_code;
};

struct SyscallClockGettime {
  u64_as_string which_clock;
  u64_as_string return_code;
  timespec data;
};

struct SyscallClockGetres {
  u64_as_string which_clock;
  u64_as_string return_code;
  std::optional<timespec> data;
};

struct SyscallGetgroups {
  u64_as_string return_code;
  std::vector<uint32_t> data;
};

struct SyscallSchedGetaffinity {
  int32_t pid;
  u64_as_string len;
  u64_as_string return_code;
  std::vector<uint8_t> affinity;
};

struct Rlimit {
  u64_as_string rlim_cur;
  u64_as_string rlim_max;
};

struct SyscallPrlimit {
  int32_t pid;
  int32_t resource;
  std::optional<Rlimit> newlimit;
  std::optional<Rlimit> oldlimit;
  u64_as_string return_code;
};

struct SyscallEpollCreate1 {
  int32_t flags;
  u64_as_string return_code;
};

using SyscallStart = std::variant<
    SyscallRead, SyscallWrite, SyscallOpen, SyscallStat, SyscallFstat,
    SyscallClose, SyscallLseek, SyscallMmap, SyscallMprotect, SyscallMunmap,
    SyscallBrk, SyscallRtSigaction, SyscallRtSigprocmask, SyscallIoctl,
    SyscallPread, SyscallReadv, SyscallWritev, SyscallPipe, SyscallGetpid,
    SyscallSocket, SyscallConnect, SyscallBind, SyscallListen,
    SyscallSetsockopt, SyscallUname, SyscallOpenat, SyscallFcntl, SyscallMkdir,
    SyscallGetuid, SyscallGeteuid, SyscallGetppid, SyscallGetgid,
    SyscallGetgroups, SyscallSchedGetaffinity, SyscallClockGettime,
    SyscallClockGetres, SyscallPrlimit, SyscallEpollCreate1, SyscallUmask>;

struct Ret {};

struct CondBr {
  bool taken;
};

struct Switch {
  u64_as_string value;
};

struct IndirectBr {
  u64_as_string addr;
};

struct Unreachable {};

using TapeEntry = std::variant<BasicBlockStart, CallInfo, SyscallStart, Ret,
                               CondBr, Switch, IndirectBr, Unreachable>;
using Tape = std::vector<TapeEntry>;

void from_json(const nlohmann::json &j, TapeEntry &obj);

}; // namespace tape

// Stuff for logging tape entries.

template <>
struct fmt::formatter<tape::IOVecForSyscall>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const tape::IOVecForSyscall &iovec, FormatContext &ctx) {
    auto out =
        fmt::format("IOVecForSyscall {{ base: {}, len: {}, data: [{}] }}",
                    iovec.base, iovec.len, fmt::join(iovec.data, ", "));
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

template <>
struct fmt::formatter<tape::Rlimit> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const tape::Rlimit &rlim, FormatContext &ctx) {
    auto out = fmt::format("Rlimit {{ rlim_cur: {}, rlim_max: {} }}",
                           rlim.rlim_cur, rlim.rlim_max);
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

template <>
struct fmt::formatter<struct stat> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const struct stat &data, FormatContext &ctx) {
    auto out = fmt::format(
        "Stat {{ st_dev: {}, st_ino: {}, st_nlink: {}, st_mode: {}, "
        "st_uid: {}, st_gid: {}, st_rdev: {}, st_size: {}, st_blksize: {}, "
        "st_blocks: {}, st_atime: {}, st_atime_nsec: {}, st_mtime: {}, "
        "st_mtime_nsec: {}, st_ctime: {}, st_ctime_nsec: {} }}",
        data.st_dev, data.st_ino, data.st_nlink, data.st_mode, data.st_uid,
        data.st_gid, data.st_rdev, data.st_size, data.st_blksize,
        data.st_blocks, data.st_atime, data.st_atim.tv_nsec,
        data.st_mtim.tv_nsec, data.st_mtim.tv_nsec, data.st_ctime,
        data.st_ctim.tv_nsec);
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

template <>
struct fmt::formatter<struct timespec> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const struct timespec &data, FormatContext &ctx) {
    auto out = fmt::format("timespec {{ tv_sec : {}, tv_nsec: {}}}",
                           data.tv_sec, data.tv_nsec);
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

template <>
struct fmt::formatter<tape::UTSName> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const tape::UTSName &data, FormatContext &ctx) {
    auto out = fmt::format("utsname {{ sysname :{}, nodename: {}, release: "
                           "{}, version: {}, machine: {}, domainname: {} }}",
                           data.sysname, data.nodename, data.release,
                           data.version, data.machine, data.domainname);
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

#define LMCAS_DEF_TAPE_FORMATTER(TYPE, ARG_NAME, FMT_STRING, ...)              \
  template <> struct fmt::formatter<TYPE> : fmt::formatter<std::string_view> { \
    template <typename FormatContext>                                          \
    auto format(const TYPE &ARG_NAME, FormatContext &ctx) {                    \
      auto out = fmt::format(FMT_STRING __VA_OPT__(, ) __VA_ARGS__);           \
      return fmt::formatter<string_view>::format(out, ctx);                    \
    }                                                                          \
  }

template <>
struct fmt::formatter<struct sigaction> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const struct sigaction &sigaction, FormatContext &ctx) {
    auto out = fmt::format("sigaction {{ sa_sigaction: {}, sa_mask: {}, "
                           "sa_flags: {}, sa_restorer: {} }}",
                           (void *)sigaction.sa_sigaction,
                           fmt::join(sigaction.sa_mask.__val, ", "),
                           sigaction.sa_flags, (void *)sigaction.sa_restorer);
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

template <>
struct fmt::formatter<tape::Sigset> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const tape::Sigset &sigset, FormatContext &ctx) {
    auto out = fmt::format("Sigset {{ val: {} }}", fmt::join(sigset.val, ", "));
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

template <>
struct fmt::formatter<tape::Sigaction> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const tape::Sigaction &sigaction, FormatContext &ctx) {
    auto out = fmt::format("sigaction {{ sa_handler: {}, sa_mask: {}, "
                           "sa_flags: {}, sa_restorer: {} }}",
                           sigaction.sa_handler_,
                           fmt::join(sigaction.sa_mask.val, ", "),
                           sigaction.sa_flags, sigaction.sa_restorer);
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

template <> struct fmt::formatter<sigset_t> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const sigset_t &sigset, FormatContext &ctx) {
    auto out = fmt::format("[{}]", fmt::join(sigset.__val, ", "));
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

LMCAS_DEF_TAPE_FORMATTER(tape::BasicBlockStart, entry,
                         "BasicBlockStart {{ basic_block_id: {} }}",
                         entry.basic_block_id);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallRead, entry,
    "SyscallRead {{ fd: {}, count: {}, data: [{}], return_code: {} }}",
    entry.fd, entry.count, fmt::join(entry.data, ", "), entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallWrite, entry,
    "SyscallWrite {{ fd: {}, data: [{}], return_code: {} }}", entry.fd,
    fmt::join(entry.data, ", "), entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallOpen, entry,
    "SyscallOpen {{ filename: {}, flags: {}, mode: {}, return_code: {} }}",
    entry.filename, entry.flags, entry.mode, entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallStat, entry,
    "SyscallStat {{ filename: {}, return_code: {}, data: {} }}", entry.filename,
    entry.return_code, entry.data);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallFstat, entry,
                         "SyscallFstat {{ fd: {}, return_code: {}, data: {} }}",
                         entry.fd, entry.return_code, entry.data);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallClose, entry,
                         "SyscallClose {{ fd: {}, return_code: {} }}", entry.fd,
                         entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallLseek, entry,
    "SyscallLseek {{ fd: {}, offset: {}, origin: {}, return_code: {} }}",
    entry.fd, entry.offset, entry.origin, entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallMmap, entry,
                         "SyscallMmap {{ addr: {}, len: {}, prot: {}, flags: "
                         "{}, fd: {}, off: {}, return_code: {} }}",
                         entry.addr, entry.len, entry.prot, entry.flags,
                         entry.fd, entry.off, entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallMprotect, entry,
    "SyscallMprotect {{ start: {}, len: {}, prot: {}, return_code: {} }}",
    entry.start, entry.len, entry.prot, entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallMunmap, entry,
    "SyscallMunmap {{ addr: {}, len: {}, return_code: {} }}", entry.addr,
    entry.len, entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallBrk, entry,
                         "SyscallBrk {{ brk: {}, return_code: {} }}", entry.brk,
                         entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallRtSigaction, entry,
    "SyscallRtSigaction {{ return_code: {}, sig: {}, "
    "act: {}, oldact: {}, sigsetsize: {}, sighandler : {}}}",
    entry.return_code, entry.sig,
    entry.act ? fmt::to_string(entry.act.value()) : "nullptr",
    entry.oact ? fmt::to_string(entry.oact.value()) : "nullptr",
    entry.sigsetsize, entry.sighandler);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallRtSigprocmask, entry,
                         "SyscallRtSigprocmask {{ return_code: {}, how: {}, "
                         "sigsetsize: {}, oset: {}, nset: {} }}",
                         entry.return_code, entry.how, entry.sigsetsize,
                         entry.oset ? fmt::to_string(entry.oset.value()) : "[]",
                         entry.nset ? fmt::to_string(entry.nset.value())
                                    : "[]");
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallIoctl, entry,
                         "SyscallIoctl {{ fd: {}, request: {}, arg0: {}, arg1: "
                         "{}, arg2: {}, arg3: {}, return_code: {} }}",
                         entry.fd, entry.request, entry.arg0, entry.arg1,
                         entry.arg2, entry.arg3, entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallPread, entry,
    "SyscallPread {{ fd: {}, count: {}, pos {}, data: [{}], return_code: {} }}",
    entry.fd, entry.count, entry.pos, fmt::join(entry.data, ", "),
    entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallReadv, entry,
                         "SyscallReadv {{ fd: {}, iov: {}, iovcnt: {}, "
                         "return_code: {}, iovs: [{}] }}",
                         entry.fd, entry.iov, entry.iovcnt, entry.return_code,
                         fmt::join(entry.iovs, ", "));
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallWritev, entry,
                         "SyscallWritev {{ fd: {}, iov: {}, iovcnt: {}, "
                         "return_code: {}, iovs: [{}] }}",
                         entry.fd, entry.iov, entry.iovcnt, entry.return_code,
                         fmt::join(entry.iovs, ", "));
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallPipe, entry,
                         "SyscallPipe {{ return_code: {}, pipefds [{}] }}",
                         entry.return_code, fmt::join(entry.pipefds, ", "));
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallGetpid, entry,
                         "SyscallGetpid {{ return_code: {} }}",
                         entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallSocket, entry,
                         "SyscallSocket {{ return_code: {}, family : {}, "
                         "type_socket: {}, protocol : {} }}",
                         entry.return_code, entry.family, entry.type_socket,
                         entry.protocol);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallConnect, entry,
                         "SyscallConnect {{ return_code: {}, fd: {}, "
                         "sockaddr_data: [{}], addrlen: {} }}",
                         entry.return_code, entry.fd,
                         fmt::join(entry.sockaddr_data, ", "), entry.addrlen);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallBind, entry,
                         "SyscallBind {{ return_code: {}, fd: {}, "
                         "sockaddr_data: [{}], addrlen: {} }}",
                         entry.return_code, entry.fd,
                         fmt::join(entry.sockaddr_data, ", "), entry.addrlen);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallListen, entry,
    "SyscallListen {{ return_code: {}, fd: {}, backlog: {} }}",
    entry.return_code, entry.fd, entry.backlog);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallSetsockopt, entry,
    "SyscallSetsockopt {{ return_code: {}, fd: {}, level: {}, "
    "optname: {}, optlen: {}, optval: {} }}",
    entry.return_code, entry.fd, entry.level, entry.optname, entry.optlen,
    entry.optval);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallUname, entry,
                         "SyscallUname {{ return_code: {}, data: [{}] }}",
                         entry.return_code, fmt::join(entry.data, ", "));
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallOpenat, entry,
                         "SyscallOpenat {{ /* TODO */ }}");
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallFcntl, entry,
                         "SyscallFcntl {{ fd: {}, command: {}, arg: {} }}",
                         entry.fd, entry.command, entry.arg);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallMkdir, entry,
    "SyscallMkdir {{ pathname: {}, mode: {}, return_code: {} }}",
    entry.pathname, entry.mode, entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallGetuid, entry,
                         "SyscallGetuid {{ return_code: {} }}",
                         entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallGeteuid, entry,
                         "SyscallGeteuid {{ return_code: {} }}",
                         entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallGetgid, entry,
                         "SyscallGetgid {{ return_code: {} }}",
                         entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallGetppid, entry,
                         "SyscallGetppid {{ return_code: {} }}",
                         entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallGetgroups, entry,
                         "SyscallGetgroups {{ return_code: {}, data: {}}}",
                         entry.return_code, fmt::join(entry.data, ", "));
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallSchedGetaffinity, entry,
                         "SyscallSchedGetaffinity {{ pid: {}, len: {}, "
                         "return_code: {}, affinity: [{}] }}",
                         entry.pid, entry.len, entry.return_code,
                         fmt::join(entry.affinity, ", "));
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallClockGettime, entry,
    "SyscallClockGettime {{ which_clock: {}, return_code: {}, data: {} }}",
    entry.which_clock, entry.return_code, entry.data);
LMCAS_DEF_TAPE_FORMATTER(
    tape::SyscallClockGetres, entry,
    "SyscallClockGetres {{ which_clock: {}, return_code: {}, data: {} }}",
    entry.which_clock, entry.return_code,
    entry.data ? fmt::to_string(entry.data.value()) : "nullptr");
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallPrlimit, entry,
                         "SyscallPrlimit {{ pid: {}, resource: {}, "
                         "return_code: {}, newlimit: {}, oldlimit: {} }}",
                         entry.pid, entry.resource, entry.return_code,
                         entry.newlimit.has_value()
                             ? fmt::to_string(entry.newlimit.value())
                             : "nullptr",
                         entry.oldlimit.has_value()
                             ? fmt::to_string(entry.oldlimit.value())
                             : "nullptr");
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallEpollCreate1, entry,
                         "SyscallEpollCreate1 {{ flags: {}, return_code: {} }}",
                         entry.flags, entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(tape::SyscallUmask, entry,
                         "SyscallUmask {{ return_code: {} }}",
                         entry.return_code);
LMCAS_DEF_TAPE_FORMATTER(tape::CallInfo, entry, "CallInfo {{ start: {} }}",
                         entry.start);
LMCAS_DEF_TAPE_FORMATTER(tape::Ret, entry, "Ret {{}}");
LMCAS_DEF_TAPE_FORMATTER(tape::CondBr, entry, "CondBr {{ taken: {} }}",
                         entry.taken);
LMCAS_DEF_TAPE_FORMATTER(tape::Switch, entry, "Switch {{ value: {} }}",
                         entry.value);
LMCAS_DEF_TAPE_FORMATTER(tape::IndirectBr, entry, "IndirectBr {{ addr: {} }}",
                         entry.addr);
LMCAS_DEF_TAPE_FORMATTER(tape::Unreachable, entry, "Unreachable {{}}");

#undef LMCAS_DEF_TAPE_FORMATTER

template <>
struct fmt::formatter<tape::SyscallStart> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const tape::SyscallStart &entry, FormatContext &ctx) {
    auto out = fmt::format(
        "SyscallStart::{}",
        std::visit([](const auto &entry) { return fmt::to_string(entry); },
                   entry));
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

template <>
struct fmt::formatter<tape::TapeEntry> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const tape::TapeEntry &entry, FormatContext &ctx) {
    auto out = fmt::format(
        "TapeEntry::{}",
        std::visit([](const auto &entry) { return fmt::to_string(entry); },
                   entry));
    return fmt::formatter<string_view>::format(out, ctx);
  }
};

#endif
