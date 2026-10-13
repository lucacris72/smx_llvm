# Step5, ex2: correttezza e cicli su RTL

```sh
make -s -C examples/step5/ex2 check
```

Servono la build LLVM course, il submodule hardware inizializzato, Verilator,
Python >= 3.10 e il runtime GNU RISC-V in `/opt/riscv`. La compilazione usa lo stesso
`../ex1/extrema.c` a `-O2`, una volta con `rv32imc_zicsr`, una con
`rv32imc_zicsr_xsmx`. Il driver C, startup e librerie sono identici, senza LTO.
I warning del runtime GNU durante il linking sono conservati in `build/link-*.log`.

## Correttezza

Ogni firmware esegue 6.512 chiamate: dieci varianti del kernel, otto pattern,
22 lunghezze fra 0 e 256 e offset 0, 1, 2, 3. La variante con allineamento
dichiarato viene chiamata soltanto con offset 0. Per N=0 si passa NULL.
I pattern includono costanti, -128/127, estremi solo nei primi elementi o
solo nella coda, e seed che limitano il risultato anche dopo l'intero loop.
Byte sentinella esterni al vettore rendono osservabili molti possibili
caricamenti fuori intervallo.

Il firmware confronta ogni risultato con un riferimento C scalare.
Python ricostruisce indipendentemente i valori attesi e verifica conteggi e
hash di tutti i risultati per pattern, quindi confronta i due firmware.
Controlla inoltre che il disassembly SMX contenga le dieci CONTX attese e
quello scalare nessuna. Risultati mancanti, duplicati, errati o senza PASS
fanno fallire il comando. Gli otto unit test controllano anche questi errori
del parser e alcuni casi limite del riferimento.

## Cicli

Dopo una chiamata di riscaldamento, `mcycle` misura 16 chiamate per ciascuna
combinazione temporizzata: N=63/256, offset 0..3, min, max, min/max e min/max
da `a[0]`. La variante con allineamento noto aggiunge due misure a offset 0.
Sono 34 confronti. Il tratto misurato include dispatch, chiamate, ciclo di
ripetizione e contatore; preparazione, riferimento e stampa sono esterni.
Speedup = cicli scalari / cicli SMX.

Il modello RTL usa il driver C++ event-driven condiviso con step4:
`examples/sim_event_main.cpp` e `examples/sim_support.py`. Si riutilizza lo
stesso modello Verilator e il suo conteggio dei cicli; il tempo del processo
host non entra nel benchmark. La traccia istruzioni viene diretta a
`/dev/null`; log della simulazione e dump RAM restano in `build/run-*/`.

Il terminale mostra il riepilogo per N=256/offset 0. I dati completi sono in
`build/results.json` e `build/report.md`; il risultato del run verificato e'
riportato nel [README step5](../readme.md#risultato-verificato).

Con `a[0]` il primo indice del loop e' 1: anche una base allineata puo'
richiedere un prefisso scalare, seguito da una coda. Questo spiega parte del
divario rispetto alla riduzione che parte dall'indice 0. Per piccoli N il
costo dei controlli e' significativo; non si assume uno speedup universale.
