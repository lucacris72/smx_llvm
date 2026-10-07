# Step4: builtin LLVM e benchmark con riferimenti storici

Conserviamo FP32, software Int8 e SMX della repo hardware come **ground truth**;
misuriamo soltanto SMX compilato con Clang/builtin e aggiungiamo la nuova curva.
Il vecchio benchmark che rieseguiva tutti e tre i kernel e' rimosso.
Non sono necessarie altre modifiche o ricompilazioni di LLVM/Clang o RTL.

## Uso

Dalla root del worktree `smx_llvm_course`:

```sh
make -s -C examples/step4        # IR e assembly, N=63
make -s -C examples/step4 run    # Sweep Verilator, risultati e immagini
make -s -C examples/step4 plots  # Solo immagini dai risultati salvati
make -s -C examples/step4 check  # Test automatici + sweep + immagini
make -s -C examples/step4 N=256  # Cambia solo IR/assembly
```

Servono i tool course in `build/llvm-course/bin`, runtime GNU RISC-V in
`/opt/riscv`, submodule hardware inizializzato, Verilator e Python >= 3.12.
`run`, `plots` e `check` installano automaticamente le dipendenze grafiche
di `requirements.txt` in `build/venv/` (rete necessaria al primo setup).
La compilazione resta silenziosa; comandi ed errori sono in `build/build.log`.
`clean` elimina `build/`, ma conserva i risultati e le immagini versionati.

## IR E Assembly

Lo stesso `step0/src/softmax_test.c` produce a `-O2`, senza metadata di debug:

- `build/softmax.before.ll`: intrinsic prima delle pass (`-disable-llvm-passes`).
- `build/softmax.ll`: IR ottimizzata.
- `build/softmax.s`: assembly builtin, con mnemonici `smx.*`.
- `build/softmax.inline.s`: inline assembly volatile, con gli stessi mnemonici.

I due assembly usano Clang, Xsmx e le stesse opzioni; cambia solo la selezione
dei wrapper. `N` e' un intero >= 4, default **63**, e aggiorna questi file senza
`clean`. Non cambia lo sweep, neppure con `make ... run N=256`.
Per l'IR volatile di confronto: `make -s --no-print-directory -C examples/step0 ir`.
Il percorso GCC originale di step0 continua a usare `.insn`.

## Metodo

`ground_truth.csv` e' una copia numericamente invariata del CSV storico
`example_tb/core/softmax_performance.csv` della checkout hardware locale.
I punti N=8, 64 e 1024 coincidono con la tabella nel [README hardware](../../hardware/smx/README.md#performance-summary).
Il CSV e' versionato: non occorre rigenerarlo o usare un'altra checkout.

`benchmark.c` riprende input, quantizzazione e tre pass packed di
`hardware/smx/example_tb/core/custom/softmax_comparison.c`, sostituendo le
istruzioni inline con builtin. Compiliamo con `-O3`, `rv32imc_zicsr_xsmx`,
ABI `ilp32`, per N=8,16,32,64,128,256,512,1024 (tutti senza coda).
Misuriamo una chiamata con `mcycle64` e barriere del compilatore; preparazione,
quantizzazione, stampa e validazione sono escluse, overhead del contatore incluso.
Python controlla ogni parola packed con il modello LUT e verifica il Top-1.
Non compiliamo o rieseguiamo FP32/software per misurarne i cicli.

Il driver C++ event-driven riusa il modello Verilator senza modificare il RTL;
per N=8 controlliamo anche uguaglianza di cicli e dump RAM col driver originale.
Firmware, disassembly e log rimangono in `build/bench/<N>/`.

## Risultati E Grafici

`results/` contiene CSV, JSON con provenienza/hash e [tabella completa](results/README.md).
`results/plots/` raccoglie PNG e SVG: `cycles`, `speedup`, `softmax_performance`,
le rispettive varianti `_linear` (asse X lineare) e `smx_only_linear`
(solo SMX storico/LLVM, entrambi gli assi lineari). Nei grafici completi i cicli
restano su Y logaritmica; gli speedup usano sempre i numeratori storici.

Run verificato il 2026-10-07, Clang 18.1.8. Speedup = cicli riferimento / LLVM:

| N | SMX storico | SMX LLVM | SMX / LLVM | FP32 / LLVM | SW / LLVM |
|---:|---:|---:|---:|---:|---:|
| 16 | 41 | 55 | 0.75x | 505.16x | 10.33x |
| 64 | 112 | 107 | 1.05x | 1030.62x | 26.64x |
| 128 | 491 | 253 | 1.94x | 871.03x | 22.59x |
| 1024 | 4882 | 4758 | 1.03x | 369.81x | 9.80x |

PASS su tutte le otto dimensioni. Un rapporto < 1 indica un peggioramento.
E' un confronto con risultati storici, non una misura isolata dell'effetto
dei builtin: cambiano compilatore e harness. Il riferimento FP32 e' soft-float.
