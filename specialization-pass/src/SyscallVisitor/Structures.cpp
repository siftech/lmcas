// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "../SyscallVisitor.h"
#include "FmtLlvm.h"

static void makeFieldStore(llvm::IRBuilder<> &builder, llvm::Value *ptr,
                           llvm::StructType *structType,
                           unsigned int fieldIndex, llvm::Value *value) {
  auto fieldPtr = builder.CreateStructGEP(structType, ptr, fieldIndex);
  builder.CreateStore(value, fieldPtr);
}

static void makeFieldStore(llvm::IRBuilder<> &builder, llvm::Value *ptr,
                           llvm::StructType *structType,
                           unsigned int fieldIndex, uint32_t value) {
  return makeFieldStore(builder, ptr, structType, fieldIndex,
                        builder.getInt32(value));
}

static void makeFieldStore(llvm::IRBuilder<> &builder, llvm::Value *ptr,
                           llvm::StructType *structType,
                           unsigned int fieldIndex, uint64_t value) {
  return makeFieldStore(builder, ptr, structType, fieldIndex,
                        builder.getInt64(value));
}

void SyscallVisitor::makeStore(const timespec &val, llvm::Value *ptr) {
  // TODO: Investigate why the timespec struct ends up as timeval...
  auto timespecType = llvm::StructType::getTypeByName(
      builder.getContext(), llvm::StringRef("struct.timespec"));
  if (timespecType == nullptr) {
    timespecType = llvm::StructType::getTypeByName(
        builder.getContext(), llvm::StringRef("struct.timeval"));
  }
  // If we still cant find a type, assert failure.
  if (timespecType == nullptr) {
    throw std::logic_error("Could not find struct.timeval or struct.timespec.");
  }
  ptr = builder.CreatePointerCast(ptr, llvm::PointerType::get(timespecType, 0));

  makeFieldStore(builder, ptr, timespecType, 0,
                 static_cast<uint64_t>(val.tv_sec));
  makeFieldStore(builder, ptr, timespecType, 1,
                 static_cast<uint64_t>(val.tv_nsec));
}

void SyscallVisitor::makeStore(const struct stat &val, llvm::Value *ptr) {
  auto statType =
      llvm::StructType::getTypeByName(builder.getContext(), "struct.stat");
  ptr = builder.CreatePointerCast(ptr, llvm::PointerType::get(statType, 0));

  makeFieldStore(builder, ptr, statType, 0, val.st_dev);
  makeFieldStore(builder, ptr, statType, 1, val.st_ino);
  makeFieldStore(builder, ptr, statType, 2, val.st_nlink);
  makeFieldStore(builder, ptr, statType, 3, val.st_mode);
  makeFieldStore(builder, ptr, statType, 4, val.st_uid);
  makeFieldStore(builder, ptr, statType, 5, val.st_gid);
  // 6 is skipped; it's 4 bytes worth of padding.
  makeFieldStore(builder, ptr, statType, 7, val.st_rdev);
  makeFieldStore(builder, ptr, statType, 8, static_cast<uint64_t>(val.st_size));
  makeFieldStore(builder, ptr, statType, 9,
                 static_cast<uint64_t>(val.st_blksize));
  makeFieldStore(builder, ptr, statType, 10,
                 static_cast<uint64_t>(val.st_blocks));

  makeStore(val.st_atim, builder.CreateStructGEP(statType, ptr, 11));
  makeStore(val.st_mtim, builder.CreateStructGEP(statType, ptr, 12));
  makeStore(val.st_ctim, builder.CreateStructGEP(statType, ptr, 13));

  // Remaining fields are padding (24 bytes worth).
}
