//===- SMXMinLoop.cpp - Form SMX signed-byte extrema idioms ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/SMXMinLoop.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Support/Debug.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Transforms/Utils/Local.h"
#include <optional>

using namespace llvm;
using namespace PatternMatch;

#define DEBUG_TYPE "smx-min-idiom"

namespace {

struct Reduction {
  PHINode *Phi;
  IntrinsicInst *Op;
  PHINode *ExitPhi;
  bool IsMin;
};

struct ExtremaLoop {
  BasicBlock *Header;
  BasicBlock *Exit;
  BasicBlock *Preheader;
  PHINode *Index;
  SmallVector<Reduction, 2> Reductions;
  GetElementPtrInst *Ptr;
  LoadInst *Load;
  BinaryOperator *Increment;
  ICmpInst *Done;
  BranchInst *Branch;
  Value *Count;
  Value *Base;
};

static bool hasXSMX(const Function &F) {
  Attribute A = F.getFnAttribute("target-features");
  if (!A.isValid())
    return false;
  SmallVector<StringRef, 16> Features;
  A.getValueAsString().split(Features, ',');
  bool Enabled = false;
  for (StringRef Feature : Features) {
    if (Feature == "+xsmx")
      Enabled = true;
    if (Feature == "-xsmx")
      Enabled = false;
  }
  return Enabled;
}

static std::optional<ExtremaLoop> matchExtremaLoop(Loop &L) {
  if (L.getNumBlocks() != 1 || !L.getLoopPreheader() || !L.getLoopLatch())
    return std::nullopt;
  BasicBlock *Header = L.getHeader();
  BasicBlock *Exit = L.getUniqueExitBlock();
  if (!Exit || L.getLoopLatch() != Header)
    return std::nullopt;

  BasicBlock *Preheader = L.getLoopPreheader();
  PHINode *Index = nullptr;
  SmallVector<Reduction, 2> Reductions;
  LoadInst *Load = nullptr;
  bool HasMin = false, HasMax = false;
  for (PHINode &PN : Header->phis()) {
    if (PN.getNumIncomingValues() != 2)
      return std::nullopt;
    Value *Initial = PN.getIncomingValueForBlock(Preheader);
    if (PN.getType()->isIntegerTy(32)) {
      if (Index || !(match(Initial, m_Zero()) || match(Initial, m_One())))
        return std::nullopt;
      Index = &PN;
      continue;
    }
    if (!PN.getType()->isIntegerTy(8) || !L.isLoopInvariant(Initial))
      return std::nullopt;
    auto *Op = dyn_cast<IntrinsicInst>(PN.getIncomingValueForBlock(Header));
    if (!Op || (Op->getIntrinsicID() != Intrinsic::smin &&
                Op->getIntrinsicID() != Intrinsic::smax))
      return std::nullopt;
    bool IsMin = Op->getIntrinsicID() == Intrinsic::smin;
    if ((IsMin && HasMin) || (!IsMin && HasMax))
      return std::nullopt;
    HasMin |= IsMin;
    HasMax |= !IsMin;

    Value *Loaded = nullptr;
    if (Op->getArgOperand(0) == &PN)
      Loaded = Op->getArgOperand(1);
    else if (Op->getArgOperand(1) == &PN)
      Loaded = Op->getArgOperand(0);
    auto *ThisLoad = dyn_cast_or_null<LoadInst>(Loaded);
    if (!ThisLoad || !ThisLoad->getType()->isIntegerTy(8) ||
        ThisLoad->isVolatile() || ThisLoad->isAtomic() ||
        (Load && Load != ThisLoad))
      return std::nullopt;
    Load = ThisLoad;
    Reductions.push_back({&PN, Op, nullptr, IsMin});
  }
  if (!Index || Reductions.empty())
    return std::nullopt;

  auto *Increment =
      dyn_cast<BinaryOperator>(Index->getIncomingValueForBlock(Header));
  if (!Increment || !match(Increment, m_c_Add(m_Specific(Index), m_One())))
    return std::nullopt;
  auto *Ptr = dyn_cast<GetElementPtrInst>(Load->getPointerOperand());
  if (!Ptr || !Ptr->isInBounds() || Ptr->getPointerAddressSpace() != 0 ||
      !L.isLoopInvariant(Ptr->getPointerOperand()))
    return std::nullopt;
  bool BytePtr = Ptr->getSourceElementType()->isIntegerTy(8) &&
                 Ptr->getNumIndices() == 1 && Ptr->getOperand(1) == Index;
  auto *Array = dyn_cast<ArrayType>(Ptr->getSourceElementType());
  bool ByteArray = Array && Array->getElementType()->isIntegerTy(8) &&
                   Ptr->getNumIndices() == 2 &&
                   match(Ptr->getOperand(1), m_Zero()) &&
                   Ptr->getOperand(2) == Index;
  if (!BytePtr && !ByteArray)
    return std::nullopt;

  auto *Branch = dyn_cast<BranchInst>(Header->getTerminator());
  if (!Branch || !Branch->isConditional())
    return std::nullopt;
  auto *Done = dyn_cast<ICmpInst>(Branch->getCondition());
  if (!Done ||
      !((Done->getPredicate() == ICmpInst::ICMP_EQ &&
         Branch->getSuccessor(0) == Exit &&
         Branch->getSuccessor(1) == Header) ||
        (Done->getPredicate() == ICmpInst::ICMP_NE &&
         Branch->getSuccessor(0) == Header &&
         Branch->getSuccessor(1) == Exit)))
    return std::nullopt;
  Value *Count = nullptr;
  if (Done->getOperand(0) == Increment)
    Count = Done->getOperand(1);
  else if (Done->getOperand(1) == Increment)
    Count = Done->getOperand(0);
  if (!Count || !Count->getType()->isIntegerTy(32) || !L.isLoopInvariant(Count))
    return std::nullopt;

  unsigned NumExitPhis = 0;
  for (PHINode &PN : Exit->phis()) {
    ++NumExitPhis;
    if (PN.getNumIncomingValues() != 1 || PN.getIncomingBlock(0) != Header)
      return std::nullopt;
    for (Reduction &R : Reductions)
      if (PN.getIncomingValue(0) == R.Op)
        R.ExitPhi = &PN;
  }
  if (NumExitPhis != Reductions.size())
    return std::nullopt;

  SmallPtrSet<const Instruction *, 16> Allowed;
  const Instruction *CoreInstructions[] = {Index, Ptr, Load, Increment, Done,
                                           Branch};
  for (const Instruction *I : CoreInstructions)
    Allowed.insert(I);
  for (const Reduction &R : Reductions) {
    if (!R.ExitPhi)
      return std::nullopt;
    Allowed.insert(R.Phi);
    Allowed.insert(R.Op);
  }
  // Reject unrelated work and reductions over different arrays.
  for (Instruction &I : *Header)
    if (!Allowed.contains(&I) && !isa<DbgInfoIntrinsic>(&I))
      return std::nullopt;

  return ExtremaLoop{Header, Exit, Preheader, Index, std::move(Reductions), Ptr,
                     Load, Increment, Done, Branch, Count,
                     Ptr->getPointerOperand()};
}

static Value *createContext(IRBuilder<> &B, const ExtremaLoop &M, bool Initial) {
  Value *Min = B.getInt8(127), *Max = B.getInt8(-128);
  for (const Reduction &R : M.Reductions) {
    Value *V = Initial ? R.Phi->getIncomingValueForBlock(M.Preheader) : R.Op;
    // A poison seed in one reduction must not contaminate the other result.
    if (M.Reductions.size() == 2 && !isa<ConstantInt>(V))
      V = B.CreateFreeze(V);
    (R.IsMin ? Min : Max) = V;
  }
  return B.CreateOr(B.CreateShl(B.CreateZExt(Min, B.getInt32Ty()), 8),
                    B.CreateZExt(Max, B.getInt32Ty()), "smx.context");
}

static void transform(ExtremaLoop &M, Loop &L, LoopStandardAnalysisResults &AR) {
  Function *F = M.Header->getParent();
  LLVMContext &C = F->getContext();
  Type *I8 = Type::getInt8Ty(C);
  Type *I32 = Type::getInt32Ty(C);
  BasicBlock *Fast = BasicBlock::Create(C, "smx.packed", F, M.Exit);
  BasicBlock *Slow = BasicBlock::Create(C, "smx.scalar", F, M.Exit);
  BasicBlock *Merge = BasicBlock::Create(C, "smx.latch", F, M.Exit);
  L.addBasicBlockToLoop(Fast, AR.LI);
  L.addBasicBlockToLoop(Slow, AR.LI);
  L.addBasicBlockToLoop(Merge, AR.LI);

  IRBuilder<> PB(M.Preheader->getTerminator());
  Value *InitialCtx = createContext(PB, M, true);
  uint64_t Start = cast<ConstantInt>(
                       M.Index->getIncomingValueForBlock(M.Preheader))
                       ->getZExtValue();
  Align BaseAlign = getKnownAlignment(M.Base, F->getParent()->getDataLayout(),
                                      M.Preheader->getTerminator(), &AR.AC,
                                      &AR.DT);
  bool InitiallyAligned = commonAlignment(BaseAlign, Start) >= Align(4);

  auto *Ctx = PHINode::Create(I32, 2, "smx.ctx", M.Header->getFirstNonPHI());
  Ctx->addIncoming(InitialCtx, M.Preheader);
  IRBuilder<> HB(M.Branch);
  HB.SetCurrentDebugLocation(M.Reductions.front().Op->getDebugLoc());
  Value *Remaining = HB.CreateSub(M.Count, M.Index, "smx.remaining");
  Value *UsePacked = HB.CreateICmpUGE(Remaining, ConstantInt::get(I32, 8));
  // Aligned starts stay aligned after +8; the scalar tail cannot return to
  // the packed path. Otherwise scalar iterations peel at most three bytes.
  if (!InitiallyAligned) {
    Value *Address = HB.CreatePtrToInt(M.Ptr, I32);
    Value *Aligned = HB.CreateICmpEQ(HB.CreateAnd(Address, 3), HB.getInt32(0),
                                     "smx.aligned");
    UsePacked = HB.CreateAnd(UsePacked, Aligned, "smx.use.packed");
  }
  BranchInst::Create(Fast, Slow, UsePacked, M.Branch);
  MDNode *LoopMD = M.Branch->getMetadata(LLVMContext::MD_loop);
  M.Branch->eraseFromParent();
  M.Done->eraseFromParent();

  IRBuilder<> FB(Fast);
  FB.SetCurrentDebugLocation(M.Load->getDebugLoc());
  Value *P1 = FB.CreateInBoundsGEP(I8, M.Ptr, FB.getInt32(4), "smx.p1.ptr");
  Value *Word0 = FB.CreateAlignedLoad(I32, M.Ptr, Align(4), "smx.p0");
  Value *Word1 = FB.CreateAlignedLoad(I32, P1, Align(4), "smx.p1");
  FB.SetCurrentDebugLocation(M.Reductions.front().Op->getDebugLoc());
  Function *Contx =
      Intrinsic::getDeclaration(F->getParent(), Intrinsic::riscv_smx_contx);
  Value *FastCtx = FB.CreateCall(Contx, {Ctx, Word0, Word1}, "smx.next.ctx");
  SmallVector<Value *, 2> FastResults;
  for (const Reduction &R : M.Reductions) {
    Value *Bits = R.IsMin ? FB.CreateLShr(FastCtx, 8) : FastCtx;
    FastResults.push_back(FB.CreateTrunc(Bits, I8, "smx.next.extremum"));
  }
  Value *FastNext = FB.CreateAdd(M.Index, ConstantInt::get(I32, 8), "smx.next.i");
  FB.CreateBr(Merge);

  BranchInst *SlowTerm = BranchInst::Create(Merge, Slow);
  M.Load->moveBefore(SlowTerm);
  for (const Reduction &R : M.Reductions)
    R.Op->moveBefore(SlowTerm);
  M.Increment->moveBefore(SlowTerm);
  IRBuilder<> SB(SlowTerm);
  // Include peeled values before entering the first packed iteration.
  Value *SlowCtx = createContext(SB, M, false);

  IRBuilder<> MB(Merge);
  auto *NextCtx = MB.CreatePHI(I32, 2, "smx.ctx.next");
  NextCtx->addIncoming(FastCtx, Fast);
  NextCtx->addIncoming(SlowCtx, Slow);
  for (unsigned I = 0; I != M.Reductions.size(); ++I) {
    Reduction &R = M.Reductions[I];
    auto *Next = MB.CreatePHI(I8, 2, R.IsMin ? "smx.min.next" : "smx.max.next");
    Next->addIncoming(FastResults[I], Fast);
    Next->addIncoming(R.Op, Slow);
    R.Phi->setIncomingValueForBlock(M.Header, Next);
    R.Phi->setIncomingBlock(R.Phi->getBasicBlockIndex(M.Header), Merge);
    R.ExitPhi->setIncomingValue(0, Next);
    R.ExitPhi->setIncomingBlock(0, Merge);
  }
  auto *NextIndex = MB.CreatePHI(I32, 2, "smx.i.next");
  NextIndex->addIncoming(FastNext, Fast);
  NextIndex->addIncoming(M.Increment, Slow);
  Value *Done = MB.CreateICmpEQ(NextIndex, M.Count);
  BranchInst *NewBranch = MB.CreateCondBr(Done, M.Exit, M.Header);
  NewBranch->setMetadata(LLVMContext::MD_loop, LoopMD);

  M.Index->setIncomingValueForBlock(M.Header, NextIndex);
  M.Index->setIncomingBlock(M.Index->getBasicBlockIndex(M.Header), Merge);
  Ctx->addIncoming(NextCtx, Merge);

  AR.DT.recalculate(*F);
  AR.SE.forgetLoop(&L);
}

} // namespace

PreservedAnalyses SMXMinLoopPass::run(Loop &L, LoopAnalysisManager &,
                                    LoopStandardAnalysisResults &AR,
                                    LPMUpdater &) {
  Function &F = *L.getHeader()->getParent();
  if (!Triple(F.getParent()->getTargetTriple()).isRISCV32() || !hasXSMX(F) ||
      !F.getParent()->getDataLayout().isLittleEndian() ||
      F.getParent()->getDataLayout().getPointerSizeInBits() != 32 || AR.MSSA)
    return PreservedAnalyses::all();
  auto M = matchExtremaLoop(L);
  if (!M)
    return PreservedAnalyses::all();

  OptimizationRemarkEmitter ORE(&F);
  ORE.emit([&]() {
    StringRef Kind = M->Reductions.front().IsMin ? "minimum" : "maximum";
    if (M->Reductions.size() == 2)
      Kind = "min/max";
    return OptimizationRemark(DEBUG_TYPE, "FormSMXMin", M->Reductions.front().Op)
           << "formed SMX packed " << Kind << " from scalar i8 loop";
  });
  transform(*M, L, AR);
  return getLoopPassPreservedAnalyses();
}
