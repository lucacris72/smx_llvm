# Step5: min, max, min/max e load allineati

La pass `smx-min-idiom` ora riconosce riduzioni signed INT8 di minimo,
massimo e dei due estremi nello stesso loop. Accetta seed costanti o variabili,
incluso `a[0]` con scansione da indice 1. Una sola `smx.contx` aggiorna entrambi
gli estremi su otto elementi.

Il percorso packed usa due load `i32 align 4`. LLVM prova l'allineamento del
primo indirizzo quando possibile; altrimenti il percorso scalare consuma il
prefisso non allineato. Lo stesso percorso gestisce la coda. Il contesto SMX
include anche i valori incontrati nel prefisso.

## Uso

Dalla root della checkout `smx_llvm_course`, ricostruire i tool modificati:

```sh
ninja -C build/llvm-course -j4 clang opt llc
make -s check-llvm
make -s -C examples/step5               # C, IR prima/dopo e assembly
make -s -C examples/step5/ex1 check     # Verifiche di compilazione
make -s -C examples/step5 run           # Confronto scalare/SMX su Verilator
make -s -C examples/step5 check         # Compilazione, test Python e RTL
```

I Makefile dipendono anche dai binari del compilatore: ricostruire LLVM
invalida automaticamente gli artefatti dello step5. I file generati sono
nelle rispettive cartelle `build/` ignorate da Git.

- [ex1](ex1/readme.md): dieci funzioni C, trasformazione isolata con `opt`,
  compilazione automatica e confronto assembly.
- [ex2](ex2/readme.md): gli stessi sorgenti eseguiti su RTL con e senza Xsmx,
  verifica di 6.512 casi per firmware e misura dei cicli.
- [Implementazione replicabile](../../docs/step5-implementation.md): modifiche
  al matcher, al contesto e al CFG, condizioni di correttezza e test.

## Risultato verificato

Run del 2026-10-09, Clang course 18.1.8, `-O2`, stesso RTL per entrambe le
versioni. Cicli totali su 16 chiamate, N=256, puntatore con offset 0:

| Kernel | Cicli scalari | Cicli SMX | Scalare / SMX |
| --- | ---: | ---: | ---: |
| Minimo | 41688 | 7974 | 5.228x |
| Massimo | 41624 | 7990 | 5.210x |
| Minimo e massimo | 62184 | 8486 | 7.328x |
| Minimo e massimo da `a[0]` | 62024 | 12118 | 5.118x |
| Minimo e massimo con allineamento noto | 62040 | 8390 | 7.395x |

PASS su tutti i casi. `ex2/build/report.md` contiene anche N=63 e offset
1, 2, 3. Lo speedup dipende da lunghezza, prefisso, coda e lavoro scalare
eliminato. Non e' un confronto con il benchmark storico di softmax dello step4.

## Limiti del matcher

Il caso resta didattico: RV32 little-endian con Xsmx, riduzioni `i8` signed,
indice `i32` iniziale 0 o 1, incremento unitario, loop canonico single-block.
Min e max devono leggere lo stesso elemento dello stesso vettore.
Load volatile/atomici, store, riduzioni unsigned, argmin/argmax e altri lavori
nel corpo non vengono trasformati. I controlli originali per N=0/N=1 restano
nel programma; gli esempi con seed `a[0]` gestiscono esplicitamente N=0.
