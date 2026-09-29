//===- SMXMinLoop.cpp - Form SMX minimum idiom -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/SMXMinLoop.h"
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
#include <optional>

using namespace llvm;
using namespace PatternMatch;

#define DEBUG_TYPE "smx-min-idiom"

namespace {

struct MinLoop {
  BasicBlock *Header;
  BasicBlock *Exit;
  BasicBlock *Preheader;
  PHINode *Index;
  PHINode *Min;
  PHINode *ExitMin;
  GetElementPtrInst *Ptr;
  LoadInst *Load;
  IntrinsicInst *Reduce;
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

static std::optional<MinLoop> matchMinLoop(Loop &L) {
  if (L.getNumBlocks() != 1 || !L.getLoopPreheader() || !L.getLoopLatch())
    return std::nullopt;
  BasicBlock *Header = L.getHeader();
  BasicBlock *Exit = L.getUniqueExitBlock();
  if (!Exit || L.getLoopLatch() != Header)
    return std::nullopt;

  BasicBlock *Preheader = L.getLoopPreheader();
  PHINode *Index = nullptr;
  PHINode *Min = nullptr;
  unsigned NumPhis = 0;
  for (PHINode &PN : Header->phis()) {
    ++NumPhis;
    if (PN.getType()->isIntegerTy(32) &&
        match(PN.getIncomingValueForBlock(Preheader), m_Zero()))
      Index = &PN;
    if (PN.getType()->isIntegerTy(8) &&
        match(PN.getIncomingValueForBlock(Preheader), m_SpecificInt(127)))
      Min = &PN;
  }
  if (NumPhis != 2 || !Index || !Min)
    return std::nullopt;

  auto *Increment = dyn_cast<BinaryOperator>(Index->getIncomingValueForBlock(Header));
  auto *Reduce = dyn_cast<IntrinsicInst>(Min->getIncomingValueForBlock(Header));
  if (!Increment || !match(Increment, m_Add(m_Specific(Index), m_One())) ||
      !Reduce || Reduce->getIntrinsicID() != Intrinsic::smin ||
      !Reduce->getType()->isIntegerTy(8))
    return std::nullopt;

  Value *Loaded = nullptr;
  if (Reduce->getArgOperand(0) == Min)
    Loaded = Reduce->getArgOperand(1);
  else if (Reduce->getArgOperand(1) == Min)
    Loaded = Reduce->getArgOperand(0);
  auto *Load = dyn_cast_or_null<LoadInst>(Loaded);
  if (!Load || !Load->getType()->isIntegerTy(8) || Load->isVolatile() ||
      Load->isAtomic())
    return std::nullopt;
  auto *Ptr = dyn_cast<GetElementPtrInst>(Load->getPointerOperand());
  if (!Ptr || !Ptr->isInBounds() || !Ptr->getSourceElementType()->isIntegerTy(8) ||
      Ptr->getNumIndices() != 1 || Ptr->getOperand(1) != Index ||
      !isa<Argument>(Ptr->getPointerOperand()))
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
  if (!Count || !isa<Argument>(Count) || !Count->getType()->isIntegerTy(32) ||
      !L.isLoopInvariant(Count))
    return std::nullopt;

  PHINode *ExitMin = nullptr;
  unsigned NumExitPhis = 0;
  for (PHINode &PN : Exit->phis()) {
    ++NumExitPhis;
    if (PN.getNumIncomingValues() == 1 && PN.getIncomingBlock(0) == Header &&
        PN.getIncomingValue(0) == Reduce)
      ExitMin = &PN;
  }
  if (NumExitPhis != 1 || !ExitMin)
    return std::nullopt;

  // All non-debug work in the loop must belong to the reduction being replaced.
  for (Instruction &I : *Header)
    if (&I != Index && &I != Min && &I != Ptr && &I != Load &&
        &I != Reduce && &I != Increment && &I != Done && &I != Branch &&
        !isa<DbgInfoIntrinsic>(&I))
      return std::nullopt;

  return MinLoop{Header, Exit, Preheader, Index, Min, ExitMin, Ptr, Load,
                 Reduce, Increment, Done, Branch, Count,
                 Ptr->getPointerOperand()};
}

static void transform(MinLoop &M, Loop &L, LoopStandardAnalysisResults &AR) {
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

  auto *Ctx = PHINode::Create(I32, 2, "smx.ctx", M.Header->getFirstNonPHI());
  Ctx->addIncoming(ConstantInt::get(I32, 0x7f80), M.Preheader);
  IRBuilder<> HB(M.Branch);
  HB.SetCurrentDebugLocation(M.Reduce->getDebugLoc());
  Value *Remaining = HB.CreateSub(M.Count, M.Index, "smx.remaining");
  Value *HasEight = HB.CreateICmpUGE(Remaining, ConstantInt::get(I32, 8));
  BranchInst::Create(Fast, Slow, HasEight, M.Branch);
  MDNode *LoopMD = M.Branch->getMetadata(LLVMContext::MD_loop);
  M.Branch->eraseFromParent();
  M.Done->eraseFromParent();

  IRBuilder<> FB(Fast);
  FB.SetCurrentDebugLocation(M.Load->getDebugLoc());
  Value *P0 = FB.CreateInBoundsGEP(I8, M.Base, M.Index, "smx.p0.ptr");
  Value *P1Index = FB.CreateAdd(M.Index, ConstantInt::get(I32, 4));
  Value *P1 = FB.CreateInBoundsGEP(I8, M.Base, P1Index, "smx.p1.ptr");
  Value *Word0 = FB.CreateAlignedLoad(I32, P0, Align(1), "smx.p0");
  Value *Word1 = FB.CreateAlignedLoad(I32, P1, Align(1), "smx.p1");
  FB.SetCurrentDebugLocation(M.Reduce->getDebugLoc());
  Function *Contx = Intrinsic::getDeclaration(F->getParent(), Intrinsic::riscv_smx_contx);
  Value *FastCtx = FB.CreateCall(Contx, {Ctx, Word0, Word1}, "smx.next.ctx");
  Value *FastMin = FB.CreateTrunc(FB.CreateLShr(FastCtx, 8), I8, "smx.next.min");
  Value *FastNext = FB.CreateAdd(M.Index, ConstantInt::get(I32, 8), "smx.next.i");
  FB.CreateBr(Merge);

  BranchInst *SlowTerm = BranchInst::Create(Merge, Slow);
  M.Ptr->moveBefore(SlowTerm);
  M.Load->moveBefore(SlowTerm);
  M.Reduce->moveBefore(SlowTerm);
  M.Increment->moveBefore(SlowTerm);

  IRBuilder<> MB(Merge);
  auto *NextCtx = MB.CreatePHI(I32, 2, "smx.ctx.next");
  NextCtx->addIncoming(FastCtx, Fast);
  NextCtx->addIncoming(Ctx, Slow);
  auto *NextMin = MB.CreatePHI(I8, 2, "smx.min.next");
  NextMin->addIncoming(FastMin, Fast);
  NextMin->addIncoming(M.Reduce, Slow);
  auto *NextIndex = MB.CreatePHI(I32, 2, "smx.i.next");
  NextIndex->addIncoming(FastNext, Fast);
  NextIndex->addIncoming(M.Increment, Slow);
  Value *Done = MB.CreateICmpEQ(NextIndex, M.Count);
  BranchInst *NewBranch = MB.CreateCondBr(Done, M.Exit, M.Header);
  NewBranch->setMetadata(LLVMContext::MD_loop, LoopMD);

  M.Index->setIncomingValueForBlock(M.Header, NextIndex);
  M.Index->setIncomingBlock(M.Index->getBasicBlockIndex(M.Header), Merge);
  M.Min->setIncomingValueForBlock(M.Header, NextMin);
  M.Min->setIncomingBlock(M.Min->getBasicBlockIndex(M.Header), Merge);
  Ctx->addIncoming(NextCtx, Merge);
  M.ExitMin->setIncomingValue(0, NextMin);
  M.ExitMin->setIncomingBlock(0, Merge);

  AR.DT.recalculate(*F);
  AR.SE.forgetLoop(&L);
}

} // namespace

PreservedAnalyses SMXMinLoopPass::run(Loop &L, LoopAnalysisManager &,
                                      LoopStandardAnalysisResults &AR,
                                      LPMUpdater &) {
  Function &F = *L.getHeader()->getParent();
  if (!Triple(F.getParent()->getTargetTriple()).isRISCV32() || !hasXSMX(F) ||
      !F.getParent()->getDataLayout().isLittleEndian() || AR.MSSA)
    return PreservedAnalyses::all();
  auto M = matchMinLoop(L);
  if (!M)
    return PreservedAnalyses::all();

  OptimizationRemarkEmitter ORE(&F);
  ORE.emit([&]() {
    return OptimizationRemark(DEBUG_TYPE, "FormSMXMin", M->Reduce)
           << "formed SMX packed minimum from scalar i8 loop";
  });
  transform(*M, L, AR);
  return getLoopPassPreservedAnalyses();
}
