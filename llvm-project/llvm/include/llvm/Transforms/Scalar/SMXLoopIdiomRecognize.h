//===- SMXLoopIdiomRecognize.h - SMX loop idiom recognition -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_SMXLOOPIDIOMRECOGNIZE_H
#define LLVM_TRANSFORMS_SCALAR_SMXLOOPIDIOMRECOGNIZE_H

#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class Loop;
class LPMUpdater;

class SMXLoopIdiomRecognizePass
    : public PassInfoMixin<SMXLoopIdiomRecognizePass> {
public:
  PreservedAnalyses run(Loop &L, LoopAnalysisManager &AM,
                        LoopStandardAnalysisResults &AR, LPMUpdater &U);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_SMXLOOPIDIOMRECOGNIZE_H
