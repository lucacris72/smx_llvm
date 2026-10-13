# Step5: modifiche replicabili alla pass SMX

Lo step5 estende il lavoro didattico sul ramo `codex/smx-extrema-aligned`,
creato da `main` al commit `200a9aed5`. Il punto di partenza e' la pass che
riconosce un solo minimo inizializzato a 127 e usa load con allineamento 1.

## 1. Descrivere una o due riduzioni

File: `llvm-project/llvm/lib/Transforms/Scalar/SMXMinLoop.cpp`.

`Reduction` raccoglie PHI, intrinsic, PHI di uscita e tipo di estremo.
`ExtremaLoop` conserva una lista di una o due riduzioni. `matchExtremaLoop`
accetta `llvm.smin.i8`, `llvm.smax.i8` o entrambi, con queste condizioni:

- Una sola PHI indice `i32`, iniziale 0 oppure 1 e incremento di uno.
- Una PHI `i8` per ciascun estremo, con seed invariante nel loop.
- Al massimo un minimo e un massimo, alimentati dallo stesso load `i8`.
- Accesso contiguo tramite GEP su byte o su array di byte con indice iniziale
  zero; base e limite invarianti, anche se non sono direttamente argomenti.
- Single-block, preheader, uscita unica e confronto sul prossimo indice.
- Una PHI LCSSA di uscita per riduzione; nessun lavoro estraneo al pattern.

Sono ancora esclusi load volatile/atomici, indirizzi in altri address space,
store, due riduzioni dello stesso tipo o riduzioni su vettori diversi.
La pass controlla RV32, little-endian, puntatori a 32 bit e `+xsmx` attivo.
Con MemorySSA attivo lascia il loop invariato, perche' questa trasformazione
non ne mantiene la struttura.

## 2. Costruire il contesto dai seed reali

Nello stesso file, `createContext` costruisce:

```text
context = (zero_extend(min) << 8) | zero_extend(max)
```

Il minimo iniziale viene dalla PHI del minimo, oppure vale 127 se si calcola
solo il massimo. Il massimo viene dalla PHI del massimo, oppure vale -128
se si calcola solo il minimo. Zero-extension e' intenzionale: ogni valore
signed viene conservato come pattern di otto bit nella propria lane.

I valori possono essere costanti diverse dal neutro, parametri o il load
di `a[0]` gia' eseguito dal C prima del loop. Le guardie originali per il
vettore vuoto e per il caso di un solo elemento conservano la loro semantica.

Quando si combinano due riduzioni, `freeze` evita che un seed poison di una
contamini anche il risultato indipendente dell'altra durante il packing.
Il backend non emette un'istruzione hardware per `freeze`.

## 3. Provare l'allineamento o consumare il prefisso

`getKnownAlignment` usa DataLayout, AssumptionCache e DominatorTree per
ricavare l'allineamento della base. `commonAlignment` tiene conto dell'indice
iniziale. Sono utilizzabili gli attributi del puntatore, l'allineamento di
oggetti globali e `llvm.assume` prodotto da `__builtin_assume_aligned`.

Se il primo indirizzo e' allineato a quattro byte, il test packed e' soltanto
`remaining >= 8`. Ogni iterazione packed incrementa l'indice di otto e
conserva l'allineamento. Quando iniziano le iterazioni scalari, restano meno
di otto elementi: il packed non puo' essere raggiunto nuovamente.

Altrimenti il test e':

```text
remaining >= 8 && ((uintptr_t)&a[i] & 3) == 0
```

Il ramo scalare avanza di uno finche' il puntatore diventa allineato:
servono al massimo tre elementi, se la lunghezza consente di arrivarci.
Poi il packed avanza di otto; la coda da zero a sette elementi torna scalare.
Il controllo dinamico resta nell'header, anche nelle iterazioni packed:
questa versione privilegia un CFG semplice e spiegabile.

I load sono emessi come `load i32 ... align 4` soltanto nel ramo che ha
superato il controllo. Leggono `a[i..i+3]` e `a[i+4..i+7]`: `remaining >= 8`
garantisce che siano tutti elementi del loop originale. Non si arrotonda
mai il puntatore all'indietro e non si legge il padding per completare una coda.

## 4. Una CONTX per entrambi gli estremi

Il percorso packed contiene due load e una sola chiamata
`llvm.riscv.smx.contx`. Il minimo si estrae dai bit 15:8, il massimo dai bit
7:0. Si estraggono soltanto gli estremi richiesti dal loop.

Il percorso scalare mantiene il load e gli intrinsic originali, poi
ricostruisce il contesto con i risultati aggiornati. Questo passaggio e'
necessario: il minimo o massimo globale potrebbe trovarsi soltanto nel
prefisso elaborato prima della prima istruzione SMX.

Nel merge ci sono una PHI per contesto e indice e una per ciascuna riduzione:
tre per min o max, quattro per min/max. La trasformazione aggiorna anche le
PHI dell'header e dell'uscita, registra i nuovi blocchi in LoopInfo,
ricalcola il DominatorTree e invalida i fatti di ScalarEvolution sul loop.

Le ottimizzazioni ulteriori integrate sono la fusione min/max in una sola
CONTX, l'eliminazione del test dinamico quando l'allineamento e' dimostrabile
e l'accettazione di basi invarianti derivate o globali. I load packed si
abbassano a due veri `lw`, evitando l'espansione in load di byte e shift.

## 5. Integrazione e test

Il nome pubblico `smx-min-idiom` e la classe `SMXMinLoopPass` restano quelli
usati negli step precedenti. La registrazione esistente in PassRegistry,
CMake e nella pipeline RISC-V include automaticamente l'implementazione
estesa. L'header `SMXMinLoop.h` ne aggiorna la descrizione.

File di verifica:

- `llvm-project/llvm/test/Transforms/SMXMinLoop/extrema-i8.ll`: seed, fusione, prefisso,
  allineamento noto e start=1, target esclusi, load atomico e controlli sulle
  analisi del CFG con i verifier LLVM.
- `llvm-project/clang/test/CodeGen/RISCV/smx-extrema-loop.c`: lowering automatico a
  `-O2`/`-O3`, allineamento via assume/globali, basi derivate, casi negativi
  e compilazione senza Xsmx.
- `llvm-project/llvm/test/Transforms/SMXMinLoop/min-i8.ll` ed
  `examples/step3/ex1/checks.txt`: aspettativa aggiornata da `align 1` a `align 4`.
- `examples/step5/ex1`: dieci esempi C con controlli su IR e assembly.
- `examples/step5/ex2`: equivalenza RTL per 6.512 chiamate per firmware,
  riferimento Python, unit test del parser e 34 confronti prestazionali.

Il target `make check-llvm` include il nuovo test Clang. I Makefile step5
dipendono dai binari dei tool per evitare risultati obsoleti dopo una rebuild.
Il driver event-driven gia' usato nello step4 viene condiviso attraverso
`examples/sim_event_main.cpp` e `examples/sim_support.py`.

## 6. Ricostruire e ripetere

```sh
ninja -C build/llvm-course -j4 clang opt llc
make -s check-llvm
make -s -C examples/step5 check
```

Gli artefatti di `ex1/build/` permettono di seguire ogni rappresentazione.
`ex2/build/report.md` e `results.json` riportano tutte le misure; il
[README step5](../examples/step5/readme.md) conserva il riepilogo verificato.
Lo step3 resta l'esempio introduttivo: ricompilandolo con questa pass riceve
anche il nuovo trattamento dell'allineamento.
