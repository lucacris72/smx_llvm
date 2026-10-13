# Step5, ex1: ispezione delle trasformazioni

```sh
make -s -C examples/step5/ex1
make -s -C examples/step5/ex1 check
```

`extrema.c` contiene soltanto loop C. Le funzioni `min_i8`, `max_i8` e
`minmax_i8` usano i seed neutri; le varianti `_seed_i8` accettano valori
iniziali come argomenti; le varianti `_first_i8` partono da `a[0]` e indice 1.
`minmax_aligned_i8` dichiara un allineamento noto tramite
`__builtin_assume_aligned(a, 4)`: il chiamante deve fornire un puntatore
effettivamente allineato. L'annotazione non sposta i dati.

| File in `build/` | Contenuto |
| --- | --- |
| `extrema.before.ll` | Clang `-O1`: IR canonica con `llvm.smin.i8`/`llvm.smax.i8` |
| `extrema.pass.ll` | Pass isolata, eseguita con `opt` e `-verify-each` |
| `extrema.auto.ll` | Clang `-O2`: pass nella pipeline normale |
| `extrema.auto.s` | RV32I_Xsmx: due `lw` e una `smx.contx` nel percorso packed |
| `extrema.scalar.s` | Stesso sorgente a `-O2`, senza Xsmx |
| `extrema.remarks.txt` | Diagnostica delle riduzioni riconosciute |

Per una spiegazione breve, confrontare `max_i8`, `minmax_i8`,
`minmax_first_i8` e `minmax_aligned_i8`. Nell'ultima funzione manca il test
dinamico dell'indirizzo; nella variante generica `smx.aligned` verifica i
due bit bassi del puntatore prima dei load packed.

`checks.txt` verifica tutte le dieci trasformazioni, l'unica chiamata CONTX
per min/max, i seed, l'allineamento e i veri load di parola nell'assembly.
`check` esegue anche i test LLVM/Clang dello step5 e il test IR precedente.
