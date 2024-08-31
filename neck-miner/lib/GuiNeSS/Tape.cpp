// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "GuiNeSS/Tape.h"
#include <fmt/ranges.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define FIELD(NAME) obj.NAME = j.at(TOSTRING(NAME)).get<decltype(obj.NAME)>()

namespace std {
template <typename T>
void from_json(const nlohmann::json &j, std::optional<T> &obj) {
  if (!j.is_null()) {
    obj = j.get<T>();
  } else {
    obj = std::nullopt;
  }
}
} // namespace std

namespace tape {

void from_json(const nlohmann::json &j, i64_as_string &obj) {
  obj.value = std::stoll(j.get<std::string>());
}

void from_json(const nlohmann::json &j, u64_as_string &obj) {
  obj.value = std::stoull(j.get<std::string>());
}

void from_json(const nlohmann::json &j, string_as_array &obj) {
  std::vector<char> ch_arr = j.get<std::vector<char>>();
  std::string s(ch_arr.begin(), ch_arr.end());
  obj.value = std::move(s);
}

void from_json(const nlohmann::json &j, Rlimit &obj) {
  obj = {j.at("rlim_cur").get<u64_as_string>(),
         j.at("rlim_max").get<u64_as_string>()};
}

void from_json(const nlohmann::json &j, UTSName &obj) {
  obj.sysname = j.at("sysname").get<tape::string_as_array>();
  obj.nodename = j.at("nodename").get<tape::string_as_array>();
  obj.release = j.at("release").get<tape::string_as_array>();
  obj.version = j.at("version").get<tape::string_as_array>();
  obj.machine = j.at("machine").get<tape::string_as_array>();
  obj.domainname = j.at("domainname").get<tape::string_as_array>();
}

void from_json(const nlohmann::json &j, BasicBlockStart &obj) {
  FIELD(basic_block_id);
}

void from_json(const nlohmann::json &j, CallInfo &obj) { FIELD(start); }

void from_json(const nlohmann::json &j, IOVecForSyscall &obj) {
  FIELD(base);
  FIELD(len);
  FIELD(data);
}

void from_json(const nlohmann::json &j, SyscallRead &obj) {
  FIELD(fd);
  FIELD(count);
  FIELD(data);
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallWrite &obj) {
  FIELD(fd);
  FIELD(data);
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallOpen &obj) {
  obj.filename = j.at("filename").get<string_as_array>();
  FIELD(flags);
  FIELD(mode);
  FIELD(return_code);
}

}; // namespace tape

void from_json(const nlohmann::json &j, struct stat &obj) {
  obj.st_dev = j.at("st_dev").get<tape::u64_as_string>();
  obj.st_ino = j.at("st_ino").get<tape::u64_as_string>();
  obj.st_nlink = j.at("st_nlink").get<tape::u64_as_string>();
  FIELD(st_mode);
  FIELD(st_uid);
  FIELD(st_gid);
  FIELD(__pad0);
  obj.st_rdev = j.at("st_rdev").get<tape::u64_as_string>();
  obj.st_rdev = j.at("st_rdev").get<tape::u64_as_string>();
  obj.st_size = j.at("st_size").get<tape::u64_as_string>();
  obj.st_blksize = j.at("st_blksize").get<tape::u64_as_string>();
  obj.st_atim.tv_sec = j.at("st_atime").get<tape::u64_as_string>();
  obj.st_atim.tv_nsec = j.at("st_atime_nsec").get<tape::u64_as_string>();
  obj.st_mtim.tv_sec = j.at("st_mtime").get<tape::u64_as_string>();
  obj.st_mtim.tv_nsec = j.at("st_mtime_nsec").get<tape::u64_as_string>();
  obj.st_ctim.tv_sec = j.at("st_ctime").get<tape::u64_as_string>();
  obj.st_ctim.tv_nsec = j.at("st_ctime_nsec").get<tape::u64_as_string>();
}

void from_json(const nlohmann::json &j, struct timespec &obj) {
  obj.tv_sec = j.at("tv_sec").get<tape::u64_as_string>();
  obj.tv_nsec = j.at("tv_nsec").get<tape::u64_as_string>();
}

namespace tape {

void from_json(const nlohmann::json &j, Sigset &obj) {
  obj.val[0] = j.at("__val")[0].get<u64_as_string>();
}

void from_json(const nlohmann::json &j, std::optional<Sigaction> &optObj) {
  if (!j.is_null()) {
    Sigaction obj;
    // Cant use field here cause sa_handler is a macro
    obj.sa_handler_ = j.at("sa_handler").get<decltype(obj.sa_handler_)>();
    FIELD(sa_flags);
    FIELD(sa_restorer);
    FIELD(sa_mask);
    optObj = std::make_optional<Sigaction>(obj);
  } else {
    optObj = std::nullopt;
  }
}

void from_json(const nlohmann::json &j, SyscallStat &obj) {
  obj.filename = j.at("filename").get<string_as_array>();
  FIELD(return_code);
  obj.data = j.at("data").get<struct stat>();
}

void from_json(const nlohmann::json &j, SyscallFstat &obj) {
  FIELD(fd);
  FIELD(return_code);
  obj.data = j.at("data").get<struct stat>();
}

void from_json(const nlohmann::json &j, SyscallClose &obj) {
  FIELD(fd);
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallLseek &obj) {
  FIELD(fd);
  FIELD(offset);
  FIELD(origin);
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallMmap &obj) {
  FIELD(addr);
  FIELD(len);
  FIELD(prot);
  FIELD(flags);
  FIELD(fd);
  FIELD(off);
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallMprotect &obj) {
  FIELD(start);
  FIELD(len);
  FIELD(prot);
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallMunmap &obj) {
  FIELD(addr);
  FIELD(len);
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallBrk &obj) {
  FIELD(brk);

  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallRtSigaction &obj) {
  FIELD(return_code);
  FIELD(sig);
  FIELD(sigsetsize);
  FIELD(act);
  FIELD(oact);
  FIELD(sighandler);
}

void from_json(const nlohmann::json &j, SyscallRtSigprocmask &obj) {
  FIELD(return_code);
  FIELD(how);
  FIELD(sigsetsize);
  FIELD(oset);
  FIELD(nset);
}

void from_json(const nlohmann::json &j, SyscallIoctl &obj) {
  FIELD(fd);
  FIELD(request);
  FIELD(arg0);
  FIELD(arg1);
  FIELD(arg2);
  FIELD(arg3);

  FIELD(arg0_contents);
  FIELD(return_code);
};

void from_json(const nlohmann::json &j, SyscallPread &obj) {
  FIELD(fd);
  FIELD(count);
  FIELD(pos);
  FIELD(data);
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallReadv &obj) {
  FIELD(fd);
  FIELD(iov);
  FIELD(iovcnt);

  FIELD(return_code);
  FIELD(iovs);
}

void from_json(const nlohmann::json &j, SyscallWritev &obj) {
  FIELD(fd);
  FIELD(iov);
  FIELD(iovcnt);

  FIELD(return_code);
  FIELD(iovs);
}

void from_json(const nlohmann::json &j, SyscallPipe &obj) {
  FIELD(return_code);
  FIELD(pipefds);
}

void from_json(const nlohmann::json &j, SyscallGetpid &obj) {
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallSocket &obj) {
  FIELD(return_code);
  FIELD(family);
  FIELD(type_socket);
  FIELD(protocol);
}

void from_json(const nlohmann::json &j, SyscallConnect &obj) {
  FIELD(return_code);
  FIELD(fd);
  FIELD(sockaddr_data);
  FIELD(addrlen);
}

void from_json(const nlohmann::json &j, SyscallBind &obj) {
  FIELD(return_code);
  FIELD(fd);
  FIELD(sockaddr_data);
  FIELD(addrlen);
}

void from_json(const nlohmann::json &j, SyscallListen &obj) {
  FIELD(return_code);
  FIELD(fd);
  FIELD(backlog);
}

void from_json(const nlohmann::json &j, SyscallSetsockopt &obj) {
  FIELD(return_code);
  FIELD(fd);
  FIELD(level);
  FIELD(optname);
  FIELD(optlen);
  obj.optval = j.at("optval").get<string_as_array>();
}

void from_json(const nlohmann::json &j, SyscallUname &obj) {
  FIELD(return_code);
  FIELD(data);
}

void from_json(const nlohmann::json &j, SyscallOpenat &obj) {
  // TODO
}

void from_json(const nlohmann::json &j, SyscallFcntl &obj) {
  FIELD(fd);
  FIELD(command);
  FIELD(arg);
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallMkdir &obj) {
  obj.pathname = j.at("pathname").get<string_as_array>();
  FIELD(mode);
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallUmask &obj) {
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallGetuid &obj) {
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallGeteuid &obj) {
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallGetgid &obj) {
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallClockGettime &obj) {
  FIELD(which_clock);
  FIELD(return_code);
  FIELD(data);
}

void from_json(const nlohmann::json &j, SyscallClockGetres &obj) {
  FIELD(which_clock);
  FIELD(return_code);
  FIELD(data);
}

void from_json(const nlohmann::json &j, SyscallPrlimit &obj) {
  FIELD(pid);
  FIELD(resource);
  obj.newlimit = j.at("nlim").get<decltype(obj.newlimit)>();
  obj.oldlimit = j.at("olim").get<decltype(obj.oldlimit)>();
}

void from_json(const nlohmann::json &j, SyscallEpollCreate1 &obj) {
  FIELD(flags);
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallGetppid &obj) {
  FIELD(return_code);
}

void from_json(const nlohmann::json &j, SyscallGetgroups &obj) {
  FIELD(return_code);
  FIELD(data);
}

void from_json(const nlohmann::json &j, SyscallSchedGetaffinity &obj) {
  FIELD(pid);
  FIELD(len);
  FIELD(return_code);
  FIELD(affinity);
}

void from_json(const nlohmann::json &j, SyscallStart &obj) {
  auto syscall = j.at("syscall").get<std::string>();

  if (syscall == "sys_read") {
    obj = j.get<SyscallRead>();
  } else if (syscall == "sys_write") {
    obj = j.get<SyscallWrite>();
  } else if (syscall == "sys_open") {
    obj = j.get<SyscallOpen>();
  } else if (syscall == "sys_stat") {
    obj = j.get<SyscallStat>();
  } else if (syscall == "sys_fstat") {
    obj = j.get<SyscallFstat>();
  } else if (syscall == "sys_close") {
    obj = j.get<SyscallClose>();
  } else if (syscall == "sys_lseek") {
    obj = j.get<SyscallLseek>();
  } else if (syscall == "sys_mmap") {
    obj = j.get<SyscallMmap>();
  } else if (syscall == "sys_mprotect") {
    obj = j.get<SyscallMprotect>();
  } else if (syscall == "sys_munmap") {
    obj = j.get<SyscallMunmap>();
  } else if (syscall == "sys_brk") {
    obj = j.get<SyscallBrk>();
  } else if (syscall == "sys_rt_sigaction") {
    obj = j.get<SyscallRtSigaction>();
  } else if (syscall == "sys_rt_sigprocmask") {
    obj = j.get<SyscallRtSigprocmask>();
  } else if (syscall == "sys_ioctl") {
    obj = j.get<SyscallIoctl>();
  } else if (syscall == "sys_pread") {
    obj = j.get<SyscallPread>();
  } else if (syscall == "sys_readv") {
    obj = j.get<SyscallReadv>();
  } else if (syscall == "sys_writev") {
    obj = j.get<SyscallWritev>();
  } else if (syscall == "sys_pipe") {
    obj = j.get<SyscallPipe>();
  } else if (syscall == "sys_getpid") {
    obj = j.get<SyscallGetpid>();
  } else if (syscall == "sys_socket") {
    obj = j.get<SyscallSocket>();
  } else if (syscall == "sys_connect") {
    obj = j.get<SyscallConnect>();
  } else if (syscall == "sys_bind") {
    obj = j.get<SyscallBind>();
  } else if (syscall == "sys_listen") {
    obj = j.get<SyscallListen>();
  } else if (syscall == "sys_setsockopt") {
    obj = j.get<SyscallSetsockopt>();
  } else if (syscall == "sys_uname") {
    obj = j.get<SyscallUname>();
  } else if (syscall == "sys_openat") {
    obj = j.get<SyscallOpenat>();
  } else if (syscall == "sys_fcntl") {
    obj = j.get<SyscallFcntl>();
  } else if (syscall == "sys_mkdir") {
    obj = j.get<SyscallMkdir>();
  } else if (syscall == "sys_getuid") {
    obj = j.get<SyscallGetuid>();
  } else if (syscall == "sys_geteuid") {
    obj = j.get<SyscallGeteuid>();
  } else if (syscall == "sys_getgid") {
    obj = j.get<SyscallGetgid>();
  } else if (syscall == "sys_getppid") {
    obj = j.get<SyscallGetppid>();
  } else if (syscall == "sys_getgroups") {
    obj = j.get<SyscallGetgroups>();
  } else if (syscall == "sys_sched_getaffinity") {
    obj = j.get<SyscallSchedGetaffinity>();
  } else if (syscall == "sys_clock_gettime") {
    obj = j.get<SyscallClockGettime>();
  } else if (syscall == "sys_clock_getres") {
    obj = j.get<SyscallClockGetres>();
  } else if (syscall == "sys_prlimit") {
    obj = j.get<SyscallPrlimit>();
  } else if (syscall == "sys_epoll_create1") {
    obj = j.get<SyscallEpollCreate1>();
  } else if (syscall == "sys_umask") {
    obj = j.get<SyscallUmask>();
  } else {
    spdlog::error("unrecognized SyscallStart syscall: {}", syscall);
    throw std::runtime_error{"failed to deserialize tape::SyscallStart"};
  }
}

void from_json(const nlohmann::json &j, Ret &obj) {}

void from_json(const nlohmann::json &j, CondBr &obj) { FIELD(taken); }

void from_json(const nlohmann::json &j, Switch &obj) { FIELD(value); }

void from_json(const nlohmann::json &j, IndirectBr &obj) { FIELD(addr); }

void from_json(const nlohmann::json &j, Unreachable &obj) {}

void from_json(const nlohmann::json &j, TapeEntry &obj) {
  auto type = j.at("type").get<std::string>();

  if (type == "basic_block_start") {
    obj = j.get<BasicBlockStart>();
  } else if (type == "call_info") {
    obj = j.get<CallInfo>();
  } else if (type == "syscall_start") {
    obj = j.get<SyscallStart>();
  } else if (type == "ret") {
    obj = j.get<Ret>();
  } else if (type == "cond_br") {
    obj = j.get<CondBr>();
  } else if (type == "switch") {
    obj = j.get<Switch>();
  } else if (type == "indirect_br") {
    obj = j.get<IndirectBr>();
  } else if (type == "unreachable") {
    obj = j.get<Unreachable>();
  } else {
    spdlog::error("unrecognized TapeEntry type: {}", type);
    throw std::runtime_error{"failed to deserialize tape::TapeEntry"};
  }
}

Tape load_tape_from_file(const std::string &path) {
  nlohmann::json tape_as_json;
  std::ifstream file(path);
  file >> tape_as_json;
  return tape_as_json.get<Tape>();
}

}; // namespace tape
