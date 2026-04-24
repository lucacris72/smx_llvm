Sì. Io lo imposterei come **progetto incrementale**, con un MVP chiaro e una parte avanzata opzionale. L’obiettivo non deve essere “LLVM riconosce qualunque softmax”, ma:

> **Estendere LLVM/RISC-V per supportare l’estensione SMX e mostrare due modalità d’uso: una esplicita tramite intrinsics/libreria, una automatica tramite riconoscimento di un loop idiomatico.**

Il progetto è ben motivato perché SMX espone la softmax INT8 come operazione instruction-level su core RISC-V scalari, evitando floating point, acceleratori esterni e datapath vettoriali . Inoltre l’algoritmo si presta bene alla compilazione perché è già diviso in fasi: riduzione `max/min/shift`, accumulo degli esponenziali, lookup finale tramite LUT 2D .

---

# Piano esecutivo del progetto

## Obiettivo generale

Realizzare una modifica alla toolchain LLVM che permetta di generare codice RISC-V per la custom unit **SMX**, acceleratore INT8 per softmax integrato nel CV32E40P.

Il progetto avrà tre risultati principali:

1. **Supporto backend LLVM/RISC-V alle istruzioni SMX**.
2. **Libreria C con intrinsics per generare una softmax SMX completa**.
3. **Pass LLVM sperimentale per riconoscere un idiom di loop, inizialmente `max/min reduction`, e abbassarlo a `SMX.CONTX`**.

La parte 1 e 2 sono il **minimo progetto completo**. La parte 3 è quella più interessante dal punto di vista del corso di compilatori.

---

# Fase 0 — Setup, baseline e definizione del perimetro

## Obiettivo

Preparare l’ambiente LLVM e definire esattamente cosa deve produrre il compilatore.

LLVM va trattato come una collezione di componenti per costruire compilatori, non come un singolo compilatore monolitico . Il flusso di lavoro utile sarà:

```text
C source
  ↓ clang -emit-llvm
LLVM IR (.ll/.bc)
  ↓ opt
LLVM IR ottimizzato
  ↓ llc
RISC-V assembly
  ↓ assembler/linker
ELF / simulazione
```

Questo flusso è coerente con quello mostrato nelle slide LLVM: `clang`, `opt`, `llc`, `llvm-mc/as`, `lld/ld` .

## Attività

Definire:

```text
Target: RISC-V 32 bit
Unità custom: SMX
Istruzioni: smx.contx, smx.acc, smx.exp, smx.sigm
Input: vettori int8_t
Packing: 4 elementi INT8 per registro a 32 bit
Output: assembly RISC-V con istruzioni SMX
```

Preparare anche una baseline:

```text
softmax_fp32.c
softmax_int8_lut.c
softmax_smx_asm.S
```

La baseline assembly serve come riferimento per capire se LLVM genera una sequenza equivalente.

## Deliverable

```text
D0.1 Ambiente LLVM buildato
D0.2 Test manuale clang → opt → llc
D0.3 Baseline assembly SMX funzionante
D0.4 Documento breve con semantica delle quattro istruzioni
```

---

# Fase 1 — Modellazione delle istruzioni SMX nel backend RISC-V

## Obiettivo

Fare in modo che LLVM conosca le quattro istruzioni SMX e possa stamparle in assembly.

Le quattro istruzioni sono già definite come istruzioni RISC-V custom R-type su opcode `CUSTOM0`; il campo `funct3` seleziona `SMX.CONTX`, `SMX.ACC`, `SMX.EXP`, `SMX.SIGM` .

## Attività

Modificare il backend RISC-V LLVM:

```text
llvm/lib/Target/RISCV/
llvm/lib/Target/RISCV/RISCVInstrInfo*.td
llvm/lib/Target/RISCV/RISCV.td
llvm/test/CodeGen/RISCV/
llvm/test/MC/RISCV/
```

Aggiungere le quattro istruzioni in TableGen:

```asm
smx.contx rd, rs1, rs2
smx.acc   rd, rs1, rs2
smx.exp   rd, rs1, rs2
smx.sigm  rd, rs1, rs2
```

Qui il primo obiettivo non è ancora farle emergere automaticamente da C, ma verificare che `llc`/MC sappiano emettere e riconoscere queste istruzioni.

## Test

Test MC/assembly:

```asm
# CHECK: smx.contx a0, a1, a2
# CHECK: smx.acc   a0, a1, a2
# CHECK: smx.exp   a0, a1, a2
# CHECK: smx.sigm  a0, a1, a2
```

Test encoding:

```text
Verificare opcode CUSTOM0
Verificare funct3 corretto
Verificare formato R-type
```

## Deliverable

```text
D1.1 Definizione TableGen delle 4 istruzioni
D1.2 Supporto assembly printer
D1.3 Supporto assembler/parser, se fattibile
D1.4 Test LLVM lit per istruzioni SMX
```

Questa fase è già un risultato forte: prima i benchmark usavano `.insn r` perché gli assembler standard non supportavano le istruzioni custom ; qui invece la toolchain inizia a conoscere SMX esplicitamente.

---

# Fase 2 — Intrinsics LLVM e builtin/wrapper C

## Obiettivo

Esporre le istruzioni SMX a livello C/C++ tramite intrinsics o builtin, così l’utente non deve scrivere assembly.

La motivazione è che SMX è pensata per essere software-visible: lo stato intermedio, come estremi, shift e somme parziali, è rappresentato in registri architetturali invece che nascosto in uno stato microarchitetturale .

## Attività

Definire intrinsics LLVM target-specific, ad esempio:

```llvm
declare i32 @llvm.riscv.smx.contx(i32 %ctx, i32 %p0, i32 %p1)
declare i32 @llvm.riscv.smx.acc(i32 %sum, i32 %packed, i32 %ctx)
declare i32 @llvm.riscv.smx.exp(i32 %packed, i32 %ctx)
declare i32 @llvm.riscv.smx.sigm(i32 %sum, i32 %exp)
```

Poi fornire wrapper C:

```c
static inline uint32_t smx_contx(uint32_t ctx, uint32_t p0, uint32_t p1);
static inline uint32_t smx_acc(uint32_t sum, uint32_t packed, uint32_t ctx);
static inline uint32_t smx_exp(uint32_t packed, uint32_t ctx);
static inline uint32_t smx_sigm(uint32_t sum, uint32_t exp);
```

Il punto critico è scegliere se implementare prima:

```text
A. wrapper con inline asm
B. veri LLVM intrinsics
```

Io farei entrambi in ordine:

```text
2A. inline asm per validare subito la libreria
2B. intrinsics veri per il progetto LLVM
```

Così hai sempre una fallback funzionante.

## Test

Compilare:

```c
uint32_t test(uint32_t ctx, uint32_t x, uint32_t y) {
    return smx_contx(ctx, x, y);
}
```

e verificare:

```asm
smx.contx a0, a1, a2
```

## Deliverable

```text
D2.1 Header smx_intrin.h
D2.2 Mapping intrinsic → istruzione SMX
D2.3 Test C → LLVM IR → RISC-V asm
D2.4 FileCheck su assembly generato
```

---

# Fase 3 — Libreria `smx_softmax_i8`

## Obiettivo

Implementare una funzione C di alto livello che l’utente può chiamare direttamente:

```c
void smx_softmax_i8(const int8_t *in, int8_t *out, size_t n);
```

Questa funzione deve generare la sequenza completa SMX.

## Struttura della funzione

La softmax SMX segue tre passate principali.

### Passata 1 — contesto

```c
uint32_t ctx = SMX_CTX_INIT;

for (i = 0; i < n; i += 8) {
    uint32_t p0 = load_u32(&in[i]);
    uint32_t p1 = load_u32(&in[i + 4]);
    ctx = smx_contx(ctx, p0, p1);
}
```

Questa fase corrisponde a `SMX.CONTX`, che aggiorna massimo, minimo e shift factor. La specifica dice che `rd` è letto e scritto, quindi va trattato come accumulatore di contesto .

### Passata 2 — accumulo

```c
uint32_t sum = 0;

for (i = 0; i < n; i += 4) {
    uint32_t p = load_u32(&in[i]);
    sum = smx_acc(sum, p, ctx);
}
```

`SMX.ACC` consuma un vettore packed INT8 e il contesto prodotto da `SMX.CONTX`, poi accumula i risultati della LUT esponenziale in un registro somma a 32 bit .

### Passata 3 — output

```c
for (i = 0; i < n; i += 4) {
    uint32_t p = load_u32(&in[i]);
    uint32_t e = smx_exp(p, ctx);
    uint32_t y = smx_sigm(sum, e);
    store_u32(&out[i], y);
}
```

Questa fase usa `SMX.EXP` per generare esponenziali packed e `SMX.SIGM` per produrre quattro output INT8 tramite LUT 2D.

## Gestione corner case

All’inizio imporrei precondizioni semplici:

```text
n multiplo di 8 per la fase contx
n multiplo di 4 per acc/exp/sigm
input allineato a 4 byte, se richiesto
output non aliasing con input, oppure comportamento documentato
```

Poi, se avanza tempo:

```text
fallback scalar per remainder
versione safe con memcpy per load/store packed non allineati
versione fast con __builtin_assume_aligned
```

## Deliverable

```text
D3.1 smx_softmax_i8.c
D3.2 smx_intrin.h
D3.3 test funzionale contro modello software
D3.4 confronto assembly generato vs assembly scritto a mano
D3.5 microbenchmark cicli, se integrabile col simulatore
```

---

# Fase 4 — Pass LLVM per riconoscere `max/min reduction`

## Obiettivo

Implementare un pass che riconosce una forma canonica di riduzione `max/min` su dati INT8 packed e la trasforma in `SMX.CONTX`.

Questa è la fase più compiler-oriented. Il riferimento concettuale è `LoopIdiomRecognize.cpp`: quel pass riconosce loop semplici e li trasforma in una forma non-loop quando questo porta vantaggi prestazionali . Usa analisi come `AliasAnalysis`, `LoopInfo`, `ScalarEvolution`, `MemorySSA`, `DominatorTree`, `TargetTransformInfo` e `IRBuilder` .

## Scelta progettuale

Non modificherei subito `LoopIdiomRecognize.cpp` principale. Creerei un pass nuovo:

```text
SMXLoopIdiomRecognize.cpp
```

Motivo: più semplice da isolare, testare e spiegare. Nella relazione puoi dire che è ispirato a `LoopIdiomRecognize`, ma specializzato per SMX.

## Pattern iniziale da riconoscere

Partirei da una forma canonica, non da C generico:

```c
uint32_t ctx = SMX_CTX_INIT;

for (int i = 0; i < n; i += 8) {
    uint32_t p0 = load_u32(&x[i]);
    uint32_t p1 = load_u32(&x[i + 4]);

    ctx = software_context_update(ctx, p0, p1);
}
```

Il pass riconosce:

```text
loop countable
induction variable con passo 8
due load i32 consecutivi
accumulatore ctx tramite phi node
assenza di store rilevanti nel loop
assenza di chiamate o side effect
uso finale del ctx fuori dal loop
```

e lo trasforma in:

```llvm
%ctx.next = call i32 @llvm.riscv.smx.contx(i32 %ctx, i32 %p0, i32 %p1)
```

## Analisi LLVM necessarie

Userei:

```text
LoopInfo
ScalarEvolution
DominatorTree
AliasAnalysis
MemorySSA opzionale
TargetTransformInfo opzionale
```

Questa scelta è in linea con il pass LLVM esistente, che usa proprio queste analisi per decidere quando una trasformazione di loop è legale e conveniente .

## Condizioni di legalità

Il pass deve essere molto conservativo:

```text
se il loop non ha preheader → non trasformare
se il trip count non è analizzabile → non trasformare
se ci sono store nel loop → non trasformare, almeno nella prima versione
se i load sono volatile/atomic → non trasformare
se gli accessi non sono consecutivi → non trasformare
se ctx ha usi strani nel loop → non trasformare
```

`LoopIdiomRecognize` stesso rinuncia se il loop non ha preheader, perché significa che non è stato portato in una forma canonica adatta .

## Deliverable

```text
D4.1 Nuovo pass SMXLoopIdiomRecognize
D4.2 Pattern matching per loop ctx
D4.3 Trasformazione verso intrinsic smx.contx
D4.4 Test .ll con FileCheck
D4.5 Test negativo: loop non canonico non trasformato
D4.6 Documentazione delle precondizioni
```

---

# Fase 5 — Estensione opzionale: riconoscimento `SMX.ACC`

## Obiettivo

Riconoscere un secondo idiom, legato all’accumulo degli esponenziali.

Pattern:

```c
uint32_t sum = 0;

for (int i = 0; i < n; i += 4) {
    uint32_t p = load_u32(&x[i]);
    sum = software_exp_acc(sum, p, ctx);
}
```

Trasformazione:

```llvm
%sum.next = call i32 @llvm.riscv.smx.acc(i32 %sum, i32 %p, i32 %ctx)
```

Questa fase è più delicata perché `SMX.ACC` dipende dal contesto già calcolato da `SMX.CONTX`. Quindi va affrontata solo dopo che la pipeline base funziona.

## Deliverable opzionali

```text
D5.1 Pattern per accumulo sum
D5.2 Trasformazione verso smx.acc
D5.3 Test .ll positivo/negativo
D5.4 Mini pipeline contx + acc
```

---

# Fase 6 — Validazione finale

## Obiettivo

Dimostrare che il progetto funziona su tre livelli:

```text
1. Correttezza della generazione assembly
2. Correttezza funzionale della softmax
3. Beneficio prestazionale o almeno riduzione istruzioni/cicli stimati
```

## Test suggeriti

### Test di codegen

Input C:

```c
void kernel(const int8_t *in, int8_t *out, int n) {
    smx_softmax_i8(in, out, n);
}
```

Output atteso:

```asm
smx.contx
smx.acc
smx.exp
smx.sigm
```

### Test di pass

Input LLVM IR con loop canonico.

Output atteso:

```llvm
call i32 @llvm.riscv.smx.contx
```

e assenza del vecchio corpo software.

### Test funzionale

Confrontare:

```text
softmax FP32 reference
softmax INT8 LUT software
softmax SMX generated
```

Metriche:

```text
L1 error
L2 error
Top-1 accuracy
Top-5 accuracy
```

Queste metriche sono già coerenti con la valutazione dell’unità SMX, che confronta fedeltà numerica e prestazioni ciclo-livello .

### Test prestazionale

Misurare:

```text
numero istruzioni
cicli simulati
speedup rispetto a baseline software
```

Il riferimento hardware mostra che l’implementazione SMX può ottenere speedup molto elevati, fino a 984× rispetto a FP32 e oltre 25× rispetto a software INT8 per `N = 64` .

## Deliverable

```text
D6.1 Suite di test LLVM lit
D6.2 Suite di test C funzionali
D6.3 Report con assembly generato
D6.4 Tabelle prestazionali
D6.5 Discussione limiti e casi non supportati
```

---

# Timeline consigliata

## Settimana 1 — Setup e baseline

```text
- build LLVM Debug+Assertions
- compilazione hello world RISC-V
- riproduzione assembly SMX manuale
- definizione semantica istruzioni
```

Output:

```text
ambiente funzionante + baseline
```

---

## Settimana 2 — Backend instruction support

```text
- aggiunta istruzioni SMX in TableGen
- assembly printer
- encoding
- primi test MC
```

Output:

```text
llc/llvm-mc riconoscono smx.contx/smx.acc/smx.exp/smx.sigm
```

---

## Settimana 3 — Intrinsics

```text
- definizione intrinsics
- lowering intrinsic → istruzione target
- wrapper C
- test C → assembly
```

Output:

```text
da C genero istruzioni SMX singole
```

---

## Settimana 4 — Libreria softmax

```text
- implementazione smx_softmax_i8
- gestione packing/load/store
- test funzionali
- confronto con assembly manuale
```

Output:

```text
utente chiama smx_softmax_i8 e LLVM genera codice SMX completo
```

---

## Settimana 5 — Pass `SMXLoopIdiomRecognize`, prima versione

```text
- nuovo pass
- riconoscimento loop canonico
- riconoscimento PHI ctx
- riconoscimento load packed
- inserimento intrinsic smx.contx
```

Output:

```text
loop max/min canonico → smx.contx
```

---

## Settimana 6 — Test, rifinitura, relazione

```text
- test positivi/negativi
- benchmark
- cleanup codice
- relazione tecnica
- slide/demo
```

Output:

```text
progetto consegnabile
```

---

# Struttura finale della repository

```text
llvm-project/
  llvm/
    lib/Target/RISCV/
      RISCVInstrInfoSMX.td
      ...
    lib/Transforms/Scalar/
      SMXLoopIdiomRecognize.cpp
    test/
      MC/RISCV/smx.s
      CodeGen/RISCV/smx-intrinsics.ll
      Transforms/SMXLoopIdiomRecognize/contx.ll

smx-runtime/
  include/
    smx_intrin.h
    smx_softmax.h
  src/
    smx_softmax_i8.c
    softmax_ref.c
  tests/
    test_softmax.c
    test_codegen.c
  benchmarks/
    bench_softmax.c

docs/
  design.md
  report.md
  examples.md
```

---

# MVP minimo da garantire

Se il tempo stringe, il progetto deve comunque consegnare questo:

```text
1. LLVM backend emette le quattro istruzioni SMX.
2. Header C con wrapper intrinsics.
3. Libreria smx_softmax_i8 funzionante.
4. Test che dimostrano C → assembly SMX.
5. Relazione che spiega come si inserisce nella pipeline LLVM.
```

Questo è già un progetto completo.

La parte di pass automatico è il valore aggiunto:

```text
6. Pass che riconosce una riduzione max/min canonica e la trasforma in smx.contx.
```

---

# Rischi principali e mitigazioni

## Rischio 1 — Intrinsics LLVM più complessi del previsto

Mitigazione:

```text
partire con inline asm wrapper
poi sostituire con intrinsic vero
```

Così la libreria softmax non resta bloccata.

## Rischio 2 — Pattern matching troppo fragile

Mitigazione:

```text
riconoscere solo loop canonici prodotti da un test .ll scritto a mano
poi eventualmente espandere a C reale
```

Non è una sconfitta: anche `LoopIdiomRecognize` lavora in modo conservativo e rinuncia quando non può dimostrare legalità.

## Rischio 3 — Auto-vectorization/packing fuori scope

Mitigazione:

```text
non chiedere al pass di impacchettare scalar i8 in i32
richiedere già load i32 packed
```

Questa è una scelta importante: se provi a fare anche packing automatico, il progetto rischia di esplodere.

## Rischio 4 — Remainder quando n non è multiplo di 4/8

Mitigazione:

```text
versione fast: richiede n multiplo di 8
versione safe: fallback scalar per gli ultimi elementi
```

---

# Titolo possibile

> **LLVM Support for SMX: Code Generation and Loop Idiom Recognition for a RISC-V INT8 Softmax Extension**

Oppure in italiano:

> **Supporto LLVM per SMX: generazione di codice e riconoscimento di idiomi di loop per un’estensione RISC-V dedicata alla softmax INT8**

---

La mia raccomandazione netta: **non partire dal pass**. Parti dal backend + intrinsics + libreria. Quando quella pipeline funziona, il pass diventa molto più semplice, perché il suo output sarà semplicemente “inserire una chiamata a intrinsic SMX”.
