#include "NeckID/NeckID/NeckUtils.h"
#include "NeckID/NeckID/Annotation.h"

namespace neckid {

std::string toString(Annotation ann) {
  switch (ann) {
  case ASSERT_TAINT:
    return std::string("T");
    break;
  case REFUTE_TAINT:
    return std::string("t");
    break;
  case ASSERT_DUP:
    return std::string("D");
    break;
  case REFUTE_DUP:
    return std::string("d");
    break;
  case ASSERT_LOOPTAINT:
    return std::string("L");
    break;
  case REFUTE_LOOPTAINT:
    return std::string("l");
    break;
  case ASSERT_BOUNDARY:
    return std::string("B");
    break;
  case REFUTE_BOUNDARY:
    return std::string("b");
    break;
  case ASSERT_ARTICULATION:
    return std::string("A");
    break;
  case REFUTE_ARTICULATION:
    return std::string("a");
    break;
  case ASSERT_CHOKEPOINT:
    return std::string("C");
    break;
  case REFUTE_CHOKEPOINT:
    return std::string("c");
    break;
  case ASSERT_NECK:
    return std::string("N");
    break;
  case REFUTE_NECK:
    return std::string("n");
    break;
  default:
    std::cerr << "toString(Annotation ann): bad choice\n";
    std::exit(EXIT_FAILURE);
    break;
  }
  return std::string("?");
}
std::string makeAnnotation(Annotation ann, int pass) {
  std::stringstream ss;
  ss << pass << toString(ann);
  return ss.str();
}
std::string makeAnnotation(Annotation ann, int pass, int iter) {
  std::stringstream ss;
  ss << pass << ":" << iter << toString(ann);
  return ss.str();
}

std::string getBBIdsString(llvm::BasicBlock *BB) {
  std::string str;
  llvm::raw_string_ostream stream(str);
  BB->printAsOperand(stream, false);
  stream << "&";
  const auto &bbid = getBasicBlockID(*BB);
  uint64_t id = (bbid.has_value()) ? bbid.value() : UINT64_MAX;
  stream << id;
  return str;
}

std::string getBBListAsString(std::unordered_set<llvm::BasicBlock *> &BBs) {
  std::string str;
  llvm::raw_string_ostream stream(str);
  unsigned Counter = 1;
  for (auto *BB : BBs) {
    std::string Msg =
        "BB " + std::to_string(Counter) + " in function '" +
        ((BB->getParent()->hasName()) ? BB->getParent()->getName().str()
                                      : "<unnamed function>") +
        "':\n";
    stream << Msg;
    stream << std::string(Msg.size() - 1, '=');
    stream << *BB;
    ++Counter;
  }
  return str;
}

llvm::StringRef getSafeName(llvm::Function *func) {
  llvm::StringRef funcName("???");
  if (func) {
    funcName = func->getName();
  }
  return funcName;
}

std::string getBBName(llvm::BasicBlock *Node, bool removePrefix) {
  if (!Node->getName().empty()) {
    return Node->getName().str();
  }

  std::string Str;
  llvm::raw_string_ostream OS(Str);

  Node->printAsOperand(OS, false);

  std::string result = OS.str();
  if (removePrefix) {
    result.erase(0, 1);
  }
  return result;
}

// TODO: Clean up the control flow.
static llvm::Value *translateValue(
    std::unordered_map<llvm::Value const *, llvm::Value *> const &values,
    llvm::Value *original) {
  // If we have an entry for the original value in our values map,
  // we can just return that.
  if (auto findResult = values.find(original); findResult != values.end()) {
    return std::get<1>(*findResult);

  } else if (auto metadataAsValue =
                 llvm::dyn_cast<llvm::MetadataAsValue>(original)) {
    // If the operand was a metadata-as-value, we need to check if it was a
    // value-as-metadata. If so, we might want to translate that.
    if (auto valueAsMetadataAsValue = llvm::dyn_cast<llvm::ValueAsMetadata>(
            metadataAsValue->getMetadata())) {
      auto wrappedValue = valueAsMetadataAsValue->getValue();
      auto transformedWrappedValue = translateValue(values, wrappedValue);
      // If the value was translated, we rewrap it. Otherwise, we can hand back
      // the original value.
      if (wrappedValue != transformedWrappedValue) {
        auto &mod = original->getContext();
        return llvm::MetadataAsValue::get(
            mod, llvm::ValueAsMetadata::get(transformedWrappedValue));
      } else {
        return original;
      }

    } else {
      // If not, it's just normal metadata, so we can return it.
      return original;
    }

  } else {
    // If nothing else applied, we don't need to translate the value.
    return original;
  }
}

// Merges the child basic block with the parent one, appending all of its
// instructions and deleting the child.
//
// Returns the parent, or an error about a precondition violation. See the
// MergeBBsError type for information on the preconditions.
//
// If an error occurs, no mutation is made to the parent.
MergeBBResult mergeBBs(llvm::BasicBlock *parent, llvm::BasicBlock *child) {

  // Check that the parent and child are from the same function.
  if (parent->getParent() != child->getParent())
    return MergeBBsError::DifferentFunction;

  // Check that the parent was terminated by an unconditional branch to the
  // child.
  auto parentTerminator =
      llvm::dyn_cast<llvm::BranchInst>(parent->getTerminator());

  if (!parentTerminator) {
    return MergeBBsError::ParentTerminatorNotBranch;
  }

  if (parentTerminator->isConditional()) {
    return MergeBBsError::ParentTerminatorConditional;
  }

  assert(parentTerminator->getNumSuccessors() == 1);
  auto parentSuccessor = parentTerminator->getSuccessor(0);
  if (parentSuccessor != child) {
    return MergeBBsError::ParentTerminatorNotToChild;
  }

  // Check that the child had exactly one predecessor, which was the parent.
  if (auto childPredecessor = child->getSinglePredecessor()) {
    // TODO: Should this be an assert? It feels like this should only happen
    // if LLVM's own internal invariants have been broken in a way none of our
    // code ought to do.
    if (childPredecessor != parent)
      return MergeBBsError::ChildHasWrongPredecessors;
  } else {
    return MergeBBsError::ChildHasWrongPredecessors;
  }

  // Create a map to accumulate what instructions in the child should
  // translate to.
  //
  // TODO: This should probably use absl::flat_hash_map for performance
  // reasons, once we've got time to fix up the build system.
  std::unordered_map<llvm::Value const *, llvm::Value *> values;

  // Get rid of the parent's terminator, since we'll be appending copies of
  // the child instructions to the parent.
  parentTerminator->eraseFromParent();

  // Create a builder positioned at the end of the parent, which we'll be
  // using to insert the child instructions.
  llvm::IRBuilder<> builder{parent};

  // Insert each child instruction into the parent.
  for (auto const &insn : *child) {
    // Insert the phis into the values map, essentially evaluating them away.
    if (auto phi = llvm::dyn_cast<llvm::PHINode>(&insn)) {
      assert(phi->getNumIncomingValues() == 1);
      assert(phi->getIncomingBlock(0) == parent);
      values.emplace(&insn, phi->getIncomingValue(0));

    } else {
      // Clone the child instruction.
      auto newInstruction = insn.clone();

      // Insert an entry for the instruction into the values map.
      values.emplace(&insn, newInstruction);

      // Translate each of its operands.
      for (unsigned i = 0; i < newInstruction->getNumOperands(); i++)
        newInstruction->setOperand(
            i, translateValue(values, newInstruction->getOperand(i)));

      // Append it to the parent frame.
      builder.Insert(newInstruction);
    }
  }

  // Get rid of the child.
  child->eraseFromParent();

  // Get rid of the LMCASBasicBlockID.
  parent->getTerminator()->setMetadata("LmcasBasicBlockID", nullptr);

  return parent;
}

} // namespace neckid
