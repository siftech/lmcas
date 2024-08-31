#ifndef __NECKID_NECKID_ANNOTATION_H__
#define __NECKID_NECKID_ANNOTATION_H__

#include <optional>
#include <string>

#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/raw_ostream.h"

namespace neckid {

/// Return the Basic Block id assigned by the "--assignid" pass
std::optional<uint64_t> getBasicBlockID(llvm::BasicBlock const &basicBlock);

} // namespace neckid

#endif
