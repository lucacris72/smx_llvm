//===- SMXLoopIdiomRecognize.cpp - SMX loop idiom recognition -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Recognize small, explicit SMX context-update idioms and replace them with the
// RISC-V SMX target intrinsic. This is intentionally conservative: it only
// forms llvm.riscv.smx.contx for RV32 functions with +smx.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/SMXLoopIdiomRecognize.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Support/Debug.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;
using namespace PatternMatch;

#define DEBUG_TYPE "smx-loop-idiom"

namespace {

static bool hasSMXFeature(const Function &F) {
  Attribute FSAttr = F.getFnAttribute("target-features");
  if (!FSAttr.isValid())
    return false;

  SmallVector<StringRef, 8> Features;
  FSAttr.getValueAsString().split(Features, ',');
  return is_contained(Features, "+smx");
}

static bool isRV32SMXFunction(const Function &F) {
  Triple TT(F.getParent()->getTargetTriple());
  return TT.isRISCV32() && hasSMXFeature(F);
}

static FunctionCallee getContxIntrinsic(Module &M) {
  return Intrinsic::getDeclaration(&M, Intrinsic::riscv_smx_contx);
}

static CallInst *createContxCall(Instruction *InsertBefore, Value *Ctx,
                                 Value *P0, Value *P1) {
  IRBuilder<> Builder(InsertBefore);
  Module *M = InsertBefore->getModule();
  return Builder.CreateCall(getContxIntrinsic(*M), {Ctx, P0, P1},
                            "smx.contx");
}

static bool replaceMarkerCall(CallInst &CI) {
  Function *Callee = CI.getCalledFunction();
  if (!Callee || Callee->getName() != "__smx_contx_model")
    return false;

  if (!CI.getType()->isIntegerTy(32) || CI.arg_size() != 3)
    return false;
  if (!CI.getArgOperand(0)->getType()->isIntegerTy(32) ||
      !CI.getArgOperand(1)->getType()->isIntegerTy(32) ||
      !CI.getArgOperand(2)->getType()->isIntegerTy(32))
    return false;

  CallInst *Contx =
      createContxCall(&CI, CI.getArgOperand(0), CI.getArgOperand(1),
                      CI.getArgOperand(2));
  CI.replaceAllUsesWith(Contx);
  CI.eraseFromParent();
  return true;
}

static bool matchAndConst(Value *V, uint64_t C, Value *&X) {
  const APInt *Mask = nullptr;
  if (!match(V, m_And(m_Value(X), m_APInt(Mask))))
    return false;
  return Mask->getBitWidth() <= 64 && Mask->getZExtValue() == C;
}

static bool matchShlConst(Value *V, uint64_t C, Value *&X) {
  const APInt *Shift = nullptr;
  if (!match(V, m_Shl(m_Value(X), m_APInt(Shift))))
    return false;
  return Shift->getBitWidth() <= 64 && Shift->getZExtValue() == C;
}

static void collectOrLeaves(Value *V, SmallVectorImpl<Value *> &Leaves) {
  Value *L = nullptr;
  Value *R = nullptr;
  if (match(V, m_Or(m_Value(L), m_Value(R)))) {
    collectOrLeaves(L, Leaves);
    collectOrLeaves(R, Leaves);
    return;
  }
  Leaves.push_back(V);
}

static bool containsIntrinsic(Value *Root, Intrinsic::ID ID,
                              SmallPtrSetImpl<Value *> &Visited) {
  if (!Visited.insert(Root).second)
    return false;

  if (auto *II = dyn_cast<IntrinsicInst>(Root))
    if (II->getIntrinsicID() == ID)
      return true;

  auto *I = dyn_cast<Instruction>(Root);
  if (!I)
    return false;

  for (Value *Op : I->operands())
    if (containsIntrinsic(Op, ID, Visited))
      return true;
  return false;
}

static bool containsIntrinsic(Value *Root, Intrinsic::ID ID) {
  SmallPtrSet<Value *, 32> Visited;
  return containsIntrinsic(Root, ID, Visited);
}

static bool containsValue(Value *Root, Value *Needle,
                          SmallPtrSetImpl<Value *> &Visited) {
  if (Root == Needle)
    return true;
  if (!Visited.insert(Root).second)
    return false;

  auto *I = dyn_cast<Instruction>(Root);
  if (!I)
    return false;

  for (Value *Op : I->operands())
    if (containsValue(Op, Needle, Visited))
      return true;
  return false;
}

static bool containsValue(Value *Root, Value *Needle) {
  SmallPtrSet<Value *, 32> Visited;
  return containsValue(Root, Needle, Visited);
}

static PHINode *findLoopCarriedContextPhi(Loop &L, Instruction &NextCtx) {
  BasicBlock *Header = L.getHeader();
  for (PHINode &PN : Header->phis()) {
    if (!PN.getType()->isIntegerTy(32))
      continue;
    for (unsigned I = 0, E = PN.getNumIncomingValues(); I != E; ++I)
      if (PN.getIncomingValue(I) == &NextCtx &&
          L.contains(PN.getIncomingBlock(I)))
        return &PN;
  }
  return nullptr;
}

static void collectLoads(Value *Root, Loop &L, SetVector<LoadInst *> &Loads,
                         SmallPtrSetImpl<Value *> &Visited) {
  if (!Visited.insert(Root).second)
    return;

  if (auto *LI = dyn_cast<LoadInst>(Root)) {
    if (L.contains(LI) && LI->getType()->isIntegerTy(32) && !LI->isVolatile() &&
        !LI->isAtomic())
      Loads.insert(LI);
    return;
  }

  auto *I = dyn_cast<Instruction>(Root);
  if (!I)
    return;

  for (Value *Op : I->operands())
    collectLoads(Op, L, Loads, Visited);
}

static bool collectTwoPackedLoads(Value *MaxV, Value *MinV, Loop &L,
                                  LoadInst *&P0, LoadInst *&P1) {
  SetVector<LoadInst *> Loads;
  SmallPtrSet<Value *, 32> Visited;
  collectLoads(MaxV, L, Loads, Visited);
  collectLoads(MinV, L, Loads, Visited);
  if (Loads.size() != 2)
    return false;

  P0 = Loads[0];
  P1 = Loads[1];
  return true;
}

static bool matchPackedContext(Value *V, Value *&MaxV, Value *&MinV,
                               Value *&ShiftV) {
  SmallVector<Value *, 4> Leaves;
  collectOrLeaves(V, Leaves);

  for (Value *Leaf : Leaves) {
    Value *Inner = nullptr;
    Value *Candidate = nullptr;

    if (!MaxV && matchAndConst(Leaf, 255, Candidate)) {
      MaxV = Candidate;
      continue;
    }

    if (!MinV && matchShlConst(Leaf, 8, Inner) &&
        matchAndConst(Inner, 255, Candidate)) {
      MinV = Candidate;
      continue;
    }

    if (!ShiftV && matchShlConst(Leaf, 16, Inner) &&
        matchAndConst(Inner, 15, Candidate)) {
      ShiftV = Candidate;
      continue;
    }
  }

  return MaxV && MinV && ShiftV;
}

static bool replaceScalarContextPack(Instruction &I, Loop &L) {
  if (!I.getType()->isIntegerTy(32) || I.getOpcode() != Instruction::Or)
    return false;

  Value *MaxV = nullptr;
  Value *MinV = nullptr;
  Value *ShiftV = nullptr;
  if (!matchPackedContext(&I, MaxV, MinV, ShiftV))
    return false;

  if (!containsIntrinsic(MaxV, Intrinsic::smax) ||
      !containsIntrinsic(MinV, Intrinsic::smin) ||
      !containsIntrinsic(ShiftV, Intrinsic::ctlz) ||
      !containsValue(ShiftV, MaxV) || !containsValue(ShiftV, MinV))
    return false;

  PHINode *Ctx = findLoopCarriedContextPhi(L, I);
  if (!Ctx)
    return false;

  LoadInst *P0 = nullptr;
  LoadInst *P1 = nullptr;
  if (!collectTwoPackedLoads(MaxV, MinV, L, P0, P1))
    return false;

  CallInst *Contx = createContxCall(&I, Ctx, P0, P1);
  I.replaceAllUsesWith(Contx);
  I.eraseFromParent();
  return true;
}

} // end anonymous namespace

PreservedAnalyses
SMXLoopIdiomRecognizePass::run(Loop &L, LoopAnalysisManager &AM,
                               LoopStandardAnalysisResults &AR,
                               LPMUpdater &U) {
  Function *F = L.getHeader()->getParent();
  if (!isRV32SMXFunction(*F))
    return PreservedAnalyses::all();

  SmallVector<Instruction *, 16> Worklist;
  for (BasicBlock *BB : L.blocks())
    for (Instruction &I : *BB)
      Worklist.push_back(&I);

  bool Changed = false;
  for (Instruction *I : Worklist) {
    if (!I->getParent())
      continue;

    if (auto *CI = dyn_cast<CallInst>(I)) {
      Changed |= replaceMarkerCall(*CI);
      continue;
    }

    Changed |= replaceScalarContextPack(*I, L);
  }

  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
