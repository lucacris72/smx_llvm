# SMX LLVM Implementation Status

Questo documento descrive lo stato tecnico della repo dopo l'integrazione SMX
in LLVM 18.x. L'obiettivo attuale non e' riconoscere una softmax completa, ma
fornire una toolchain RISC-V che conosce l'estensione custom SMX, espone le
istruzioni a IR/C e include un primo pass di idiom recognition per
`smx.contx`.

## Stato del flusso

Il flusso supportato oggi e':

```text
C / LLVM IR
  -> Clang builtin o LLVM intrinsic
  -> LLVM RISC-V instruction selection
  -> assembly RISC-V con mnemonici smx.*
  -> assembler/disassembler LLVM
```

I target Makefile principali sono:

```sh
make check
make check-llvm
make smx-clang-softmax-asm
```

`make check` valida il runtime C con il compilatore host e con il GCC RISC-V
esterno configurato da `RISCV_CC`. `make check-llvm` valida la parte LLVM
patchata usando gli strumenti in `build/llvm-smx/bin`. `make
smx-clang-softmax-asm` genera assembly SMX dal runtime C usando il Clang
patchato.

## Import LLVM e hardware

La repo contiene `llvm-project/` vendorizzato come sorgenti normali, non come
submodule. La base e' documentata in `docs/llvm-project-base.md`:

```text
llvm-project release/18.x
commit 3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff
```

Il repository RTL SMX e' invece un submodule in `hardware/smx`. Serve come
contesto hardware e per le simulazioni Verilator:

```sh
git submodule update --init --recursive
make smx-rtl-unit
make smx-core-hello
make smx-core-softmax
make smx-docker-unit
make smx-docker-core-softmax
```

I wrapper Makefile non modificano il flow hardware upstream: entrano nelle
directory del submodule e lanciano i target gia' presenti. I punti hardware
utili sono:

```text
hardware/smx/SMX_dev/src
hardware/smx/example_tb/core
hardware/smx/docker/shell.sh
```

## ISA SMX nel backend RISC-V

L'estensione SMX e' modellata nel backend RISC-V LLVM come feature target
`+smx`.

File principali:

```text
llvm-project/llvm/lib/Target/RISCV/RISCVFeatures.td
llvm-project/llvm/lib/Target/RISCV/RISCVInstrInfo.td
llvm-project/llvm/lib/Target/RISCV/RISCVInstrInfoSMX.td
llvm-project/llvm/lib/Support/RISCVISAInfo.cpp
```

`RISCVFeatures.td` definisce:

```text
FeatureSMX -> SubtargetFeature<"smx", "HasSMX", ...>
HasSMX     -> Predicate + AssemblerPredicate
```

`RISCVInstrInfo.td` include `RISCVInstrInfoSMX.td`, che definisce le quattro
istruzioni:

```asm
smx.contx rd, rs1, rs2
smx.acc   rd, rs1, rs2
smx.exp   rd, rs1, rs2
smx.sigm  rd, rs1, rs2
```

Tutte usano formato R-type, opcode `CUSTOM0` e `funct7 = 0`. Il `funct3`
seleziona l'operazione:

```text
0 -> smx.contx
1 -> smx.acc
2 -> smx.exp
3 -> smx.sigm
```

`smx.contx` e `smx.acc` hanno semantica accumulator-like: il registro `rd`
viene letto e scritto dall'hardware. In TableGen sono quindi modellate con una
classe a due indirizzi (`SMX_acc_rr`) che espone un output `rd_wb` vincolato
all'input `rd`:

```text
let Constraints = "$rd_wb = $rd";
```

Questo e' importante per evitare che instruction selection perda la dipendenza
dal vecchio valore del contesto/somma.

`RISCVISAInfo.cpp` include `smx` tra le estensioni ISA supportate. Senza questa
registrazione, Clang/driver accetterebbero male `-march=rv32im_smx` e la mappa
feature usata dai builtin non conterrebbe `smx`.

### Test ISA/backend

Test principali:

```text
llvm-project/llvm/test/MC/RISCV/smx-valid.s
llvm-project/llvm/test/MC/RISCV/smx-invalid.s
llvm-project/llvm/test/CodeGen/RISCV/smx-intrinsics.ll
```

Comandi:

```sh
build/llvm-smx/bin/llvm-lit -q \
  llvm-project/llvm/test/MC/RISCV/smx-valid.s \
  llvm-project/llvm/test/MC/RISCV/smx-invalid.s \
  llvm-project/llvm/test/CodeGen/RISCV/smx-intrinsics.ll
```

Il test MC verifica:

```text
parser assembly
encoding CUSTOM0/funct3
objdump/disassembler
feature gate +smx
```

Il test CodeGen verifica che gli intrinsics LLVM target-specific vengano
selezionati nelle istruzioni SMX corrispondenti.

## LLVM IR intrinsics

Gli intrinsics target-specific sono definiti in:

```text
llvm-project/llvm/include/llvm/IR/IntrinsicsRISCV.td
```

Sono:

```llvm
declare i32 @llvm.riscv.smx.contx(i32, i32, i32)
declare i32 @llvm.riscv.smx.acc(i32, i32, i32)
declare i32 @llvm.riscv.smx.exp(i32, i32)
declare i32 @llvm.riscv.smx.sigm(i32, i32)
```

I tipi sono volutamente fissi a `i32`: SMX lavora su RV32 e su registri GPR che
contengono quattro lane INT8 packed. Le proprieta' in TableGen sono:

```text
IntrNoMem
IntrSpeculatable
```

Il lowering e' in `RISCVInstrInfoSMX.td`:

```text
int_riscv_smx_contx -> SMX_CONTX
int_riscv_smx_acc   -> SMX_ACC
int_riscv_smx_exp   -> SMX_EXP
int_riscv_smx_sigm  -> SMX_SIGM
```

Per `contx` e `acc`, il primo operando dell'intrinsic diventa sia valore letto
che destinazione fisica dell'istruzione a due indirizzi.

### Test intrinsics IR

Comando:

```sh
build/llvm-smx/bin/llvm-lit -q \
  llvm-project/llvm/test/CodeGen/RISCV/smx-intrinsics.ll
```

Per debug manuale:

```sh
build/llvm-smx/bin/llc -mtriple=riscv32 -mattr=+smx \
  llvm-project/llvm/test/CodeGen/RISCV/smx-intrinsics.ll -o -
```

Output atteso: presenza di `smx.contx`, `smx.acc`, `smx.exp`, `smx.sigm`.

## Clang builtins e header C

Clang espone i builtin RISC-V SMX in:

```text
llvm-project/clang/include/clang/Basic/BuiltinsRISCV.def
llvm-project/clang/lib/CodeGen/CGBuiltin.cpp
```

Builtins:

```c
__builtin_riscv_smx_contx(unsigned, unsigned, unsigned)
__builtin_riscv_smx_acc(unsigned, unsigned, unsigned)
__builtin_riscv_smx_exp(unsigned, unsigned)
__builtin_riscv_smx_sigm(unsigned, unsigned)
```

La stringa feature in `BuiltinsRISCV.def` e':

```text
"smx,32bit"
```

Quindi il builtin e' valido solo con target RV32 e feature `+smx`. I test
diagnostici verificano sia assenza di `+smx` sia uso su RV64.

`CGBuiltin.cpp` mappa ogni builtin all'intrinsic LLVM corrispondente:

```text
RISCV::BI__builtin_riscv_smx_contx -> Intrinsic::riscv_smx_contx
RISCV::BI__builtin_riscv_smx_acc   -> Intrinsic::riscv_smx_acc
RISCV::BI__builtin_riscv_smx_exp   -> Intrinsic::riscv_smx_exp
RISCV::BI__builtin_riscv_smx_sigm  -> Intrinsic::riscv_smx_sigm
```

L'header pubblico della repo e':

```text
include/smx_intrin.h
```

Fornisce wrapper C:

```c
uint32_t smx_contx(uint32_t ctx, uint32_t p0, uint32_t p1);
uint32_t smx_acc(uint32_t sum, uint32_t packed, uint32_t ctx);
uint32_t smx_exp(uint32_t packed, uint32_t ctx);
uint32_t smx_sigm(uint32_t sum, uint32_t expv);
```

Il percorso e' scelto a compile-time:

```text
SMX_USE_LLVM_INTRINSICS=1 + __has_builtin -> Clang builtin
__riscv + SMX_USE_MNEMONICS               -> inline asm smx.*
__riscv                                  -> inline asm .insn r
host/non-RISC-V                          -> fallback no-op funzionale
```

Il fallback host permette di compilare la libreria in ambienti non RISC-V per
smoke test e integrazione, ma non implementa la semantica softmax reale.

### Test Clang/header

Test Clang:

```text
llvm-project/clang/test/CodeGen/RISCV/smx-intrinsics.c
llvm-project/clang/test/CodeGen/RISCV/smx-intrinsics-error.c
```

Comando:

```sh
build/llvm-smx/bin/llvm-lit -q \
  llvm-project/clang/test/CodeGen/RISCV/smx-intrinsics.c \
  llvm-project/clang/test/CodeGen/RISCV/smx-intrinsics-error.c
```

Test manuale header -> assembly:

```sh
printf '#include "smx_intrin.h"\nunsigned f(unsigned c,unsigned x,unsigned y){return smx_contx(c,x,y);}\n' \
  | build/llvm-smx/bin/clang -target riscv32 -march=rv32im_smx -mabi=ilp32 \
      -O2 -S -Iinclude -DSMX_USE_LLVM_INTRINSICS=1 -x c - -o -
```

Output atteso:

```asm
smx.contx
```

## Runtime `smx_softmax_i8`

La funzione runtime e':

```text
src/smx_softmax_i8.c
include/smx_softmax.h
```

Firma pubblica:

```c
void smx_softmax_i8(const int8_t *restrict in, int8_t *restrict out, size_t n);
```

La funzione implementa la struttura hardware SMX in tre passate:

```text
1. context pass: ctx = smx_contx(ctx, p0, p1), 8 byte per iterazione
2. accumulation pass: sum = smx_acc(sum, packed, ctx), 4 byte per iterazione
3. output pass: expv = smx_exp(packed, ctx), result = smx_sigm(sum, expv)
```

Precondizione attuale:

```text
n multiplo di 8
input/output almeno n byte
input/output allineati a 4 byte per il percorso veloce
```

Se `n` non e' multiplo di 8, la funzione ritorna senza scrivere output. Questa
scelta e' conservativa e mantiene il primo runtime semplice da usare come
baseline compiler/backend.

Le funzioni `smx_load_u32` e `smx_store_u32` usano `__builtin_memcpy` quando il
compilatore e' GCC/Clang. Questo evita dipendenza da `string.h` nel percorso
Clang RISC-V freestanding e permette:

```sh
make smx-clang-softmax-asm
```

Il target genera:

```text
build/smx_softmax_i8.llvm.s
```

Verifica manuale:

```sh
rg 'smx\.(contx|acc|exp|sigm)' build/smx_softmax_i8.llvm.s
```

Output atteso: almeno una istruzione per ciascuno dei quattro mnemonici.

## Repository-level lit

La repo ha una piccola suite lit locale in:

```text
tests/lit/lit.cfg.py
tests/lit/CodeGen/RISCV/smx-softmax-runtime.c
tests/lit/CodeGen/RISCV/smx-intrinsics.ll
tests/lit/MC/RISCV/smx.s
```

`tests/lit/lit.cfg.py` configura una suite `ShTest` minimale e la sostituzione
`%repo_root`, usata dai test che compilano sorgenti della repo.

`tests/lit/CodeGen/RISCV/smx-softmax-runtime.c` compila
`src/smx_softmax_i8.c` con:

```text
-target riscv32
-march=rv32im_smx
-mabi=ilp32
-DSMX_USE_LLVM_INTRINSICS=1
```

e verifica che l'assembly contenga le istruzioni SMX.

Comando aggregato:

```sh
make check-llvm
```

La lista dei test aggregati e' in `Makefile`, variabile `LLVM_SMX_LIT_TESTS`.

## SMX loop idiom recognition

Il pass sperimentale e':

```text
llvm-project/llvm/include/llvm/Transforms/Scalar/SMXLoopIdiomRecognize.h
llvm-project/llvm/lib/Transforms/Scalar/SMXLoopIdiomRecognize.cpp
```

Registrazione build/pipeline:

```text
llvm-project/llvm/lib/Transforms/Scalar/CMakeLists.txt
llvm-project/llvm/lib/Passes/PassBuilder.cpp
llvm-project/llvm/lib/Passes/PassRegistry.def
llvm-project/llvm/lib/Target/RISCV/RISCVTargetMachine.h
llvm-project/llvm/lib/Target/RISCV/RISCVTargetMachine.cpp
```

Nome pipeline:

```sh
opt -passes=smx-loop-idiom input.ll -S
```

Il pass e' un `LoopPass` del new pass manager. La funzione `run` filtra subito
su:

```text
target triple riscv32
function attribute target-features contiene +smx
```

Questo evita trasformazioni su altri target o su codice RISC-V senza SMX.

### Pattern marker/model

Il primo pattern riconosciuto e':

```llvm
%next = call i32 @__smx_contx_model(i32 %ctx, i32 %p0, i32 %p1)
```

Il pass lo sostituisce con:

```llvm
%smx.contx = call i32 @llvm.riscv.smx.contx(i32 %ctx, i32 %p0, i32 %p1)
```

La funzione marker non e' API pubblica. Vive solo nei test IR come punto di
aggancio per dimostrare la trasformazione e il flow completo verso `llc`.

### Pattern scalarizzato canonico

Il secondo pattern e' volutamente stretto. Riconosce un'espressione IR che
calcola un nuovo contesto SMX con:

```text
max = chain di llvm.smax.i32
min = chain di llvm.smin.i32
delta = max - min
shift = clamp(ctlz(delta) - LZC_LUT, -8, 7)
next_ctx = (shift & 15) << 16 | (min & 255) << 8 | (max & 255)
```

Il matcher richiede:

```text
pack finale con or/shl/and
presenza di llvm.smax.i32 nel valore max
presenza di llvm.smin.i32 nel valore min
presenza di llvm.ctlz nel valore shift
shift dipendente sia da max sia da min
phi i32 loop-carried che riceve il next ctx
esattamente due load i32 non volatile/non atomic dentro il loop
```

Se tutto combacia, il pack finale viene sostituito da:

```llvm
call i32 @llvm.riscv.smx.contx(i32 %ctx, i32 %p0, i32 %p1)
```

Il pass non riscrive loop byte-by-byte in loop packed e non prova a dimostrare
forme arbitrarie di max/min. Questo e' intenzionale: la prima versione deve
essere facile da testare e da spiegare, non un riconoscitore generale di
softmax.

### Integrazione automatica RISC-V

`RISCVTargetMachine::registerPassBuilderCallbacks` registra il pass nel
callback `LateLoopOptimizationsEPCallback`. Di conseguenza il pass parte anche
nelle pipeline new-PM default quando il `TargetMachine` e' RISC-V.

Il pass rimane comunque auto-disabilitato se la funzione non ha `+smx`.

Test pipeline:

```sh
build/llvm-smx/bin/opt -mtriple=riscv32 -mattr=+smx \
  -passes='default<O2>' \
  llvm-project/llvm/test/Transforms/SMXLoopIdiom/marker-contx.ll -S
```

### Test idiom recognition

Test:

```text
llvm-project/llvm/test/Transforms/SMXLoopIdiom/marker-contx.ll
llvm-project/llvm/test/Transforms/SMXLoopIdiom/scalar-contx.ll
llvm-project/llvm/test/Transforms/SMXLoopIdiom/no-smx.ll
llvm-project/llvm/test/Transforms/SMXLoopIdiom/rv64.ll
llvm-project/llvm/test/Transforms/SMXLoopIdiom/scalar-incomplete.ll
```

Comando:

```sh
build/llvm-smx/bin/llvm-lit -q \
  llvm-project/llvm/test/Transforms/SMXLoopIdiom
```

Copertura:

```text
marker positivo -> llvm.riscv.smx.contx
scalar positivo -> llvm.riscv.smx.contx
senza +smx -> nessuna trasformazione
riscv64 -> nessuna trasformazione
max/min senza shift -> nessuna trasformazione
marker -> llc -> smx.contx
default<O2> -> pass attivo via callback RISC-V
```

## Build e verifica completa

Build consigliata:

```sh
cmake -S llvm-project/llvm -B build/llvm-smx \
  -G Ninja \
  -DLLVM_ENABLE_PROJECTS=clang \
  -DLLVM_TARGETS_TO_BUILD=RISCV \
  -DCMAKE_BUILD_TYPE=Release

ninja -C build/llvm-smx \
  opt clang llc llvm-mc llvm-objdump FileCheck count not llvm-readobj
```

Verifica completa SMX:

```sh
make check-llvm
make smx-clang-softmax-asm
make check
```

Verifica dei mnemonici generati dal runtime:

```sh
rg 'smx\.(contx|acc|exp|sigm)' build/smx_softmax_i8.llvm.s
```

Verifica rapida pass:

```sh
build/llvm-smx/bin/opt -passes=smx-loop-idiom -S \
  llvm-project/llvm/test/Transforms/SMXLoopIdiom/marker-contx.ll \
  | rg 'llvm\.riscv\.smx\.contx'
```

## Prossimi step tecnici

1. Raffinare il pattern scalarizzato `smx.contx`.
   Il matcher attuale e' test-driven e conservativo. Il prossimo passo e'
   generare IR da una funzione C modello e aggiungere supporto alle forme
   prodotte realmente da Clang a `-O1/-O2`.

2. Aggiungere remark diagnostici.
   Usare `OptimizationRemarkEmitter` per emettere remark quando il pass forma
   `llvm.riscv.smx.contx` e missed-remark per pattern quasi validi. Questo
   aiuterebbe molto nella relazione e nel debug.

3. Pulizia del dead code dopo sostituzione scalarizzata.
   Oggi il pass sostituisce il pack finale. Le istruzioni intermedie diventano
   spesso dead e vengono eliminate da pass successivi. Si puo' aggiungere una
   cancellazione locale piu' aggressiva, facendo attenzione a non rompere
   LCSSA/dominator invariants.

4. Estendere la coverage negativa.
   Aggiungere test per load volatile, load atomic, tre load invece di due,
   pack con shift mancante, target feature `-smx`, e loop con store.

5. Valutare `smx.acc`.
   Dopo `contx`, il candidato naturale e' `acc`: loop-carried `sum`, input
   packed i32, ctx invariant prodotto dalla passata `contx`. Il pattern e'
   piu' dipendente dalla semantica LUT, quindi conviene partire da un marker
   model come fatto per `contx`.

6. Collegare a simulazione Verilator.
   Quando il runtime e il pass generano assembly stabile, compilare un kernel
   con il Clang patchato e lanciarlo nel flow `hardware/smx/example_tb/core`.
   Questo chiude il cerchio compiler -> binary -> core RTL.

7. CI selettiva.
   Automatizzare almeno `make check-llvm` in CI. Le simulazioni Verilator
   restano opzionali finche' tempi e dipendenze non sono stabilizzati.

