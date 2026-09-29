# SMX in LLVM: dall'assembly al loop C

## Domanda del progetto

Ho integrato un acceleratore INT8 nella pipeline di un core RISC-V. Come si
modifica LLVM affinche' il software possa usare le sue istruzioni, prima in
modo esplicito e poi partendo da un loop C ordinario?

Questo e' un progetto didattico sul percorso fra ISA, assembler, IR LLVM,
backend e pass di ottimizzazione. Non e' un compilatore general purpose per
softmax e non richiede uno speedup per essere considerato riuscito.

## Tappe dimostrabili

1. **Baseline**: il firmware usa `.insn r` e GCC produce un ELF; la
   disassemblazione mostra opcode custom senza mnemonici SMX.
2. **ISA in LLVM**: `-march=rv32i_xsmx` abilita `smx.contx`, `smx.acc`,
   `smx.exp` e `smx.sigm`. `llvm-mc` li assembla, `llvm-objdump` li mostra,
   l'attributo RISC-V nell'oggetto registra `xsmx`.
3. **C esplicito**: wrapper C, builtin Clang e intrinsic LLVM emettono
   istruzioni SMX senza assembly inline. `-g` conserva simboli ELF e righe
   sorgente DWARF per il debug del firmware.
4. **C automatico**: un LoopPass dedicato riconosce un minimo signed INT8
   canonico e usa `smx.contx` su otto byte per volta. Il codice scalare
   gestisce la coda, incluso `n < 8`.

Per ogni tappa: sorgente, comando, IR/assembly/ELF prima e dopo, test e una
breve spiegazione del livello LLVM modificato. I commit seguono lo stesso
ordine e non importano il vecchio pass sperimentale.

## Base riproducibile

La branch `course/smx-from-scratch` parte dal commit `e8426c15b` di LLVM 18
e conserva l'RTL come submodule al commit `1d8dff15a190e3e14fa3d1513bc31d5247eee2b7`.
La vecchia checkout `smx_llvm` resta il riferimento legacy. `build/` e' locale
e non versionato.

```sh
git submodule update --init hardware/smx
make check
make -C hardware/smx/example_tb/core custom/softmax_comparison.elf
cmake -S llvm-project/llvm -B build/llvm-course -G Ninja \
  -DLLVM_ENABLE_PROJECTS=clang -DLLVM_TARGETS_TO_BUILD=RISCV \
  -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_BUILD_TESTS=ON
ninja -C build/llvm-course -j8 clang opt llc llvm-mc llvm-objdump \
  llvm-readelf llvm-dwarfdump FileCheck not \
  count llvm-config
```

Il nome dell'estensione e' `xsmx`, secondo la convenzione RISC-V per
estensioni custom. Il target resta `riscv32-unknown-elf`: non serve definire
una nuova architettura LLVM o un nuovo formato ELF.

## Verifiche

```sh
make check-llvm
make smx-clang-softmax-asm
make smx-min-core-elf
make smx-min-core-run
build/llvm-course/bin/llvm-readelf -A build/smx_min_i8.llvm.o
build/llvm-course/bin/llvm-objdump --mattr=+xsmx --source \
  --line-numbers -d build/smx_min_i8.llvm.o
build/llvm-course/bin/llvm-dwarfdump --debug-line \
  build/smx_min_i8.llvm.o
```

Il firmware emette `PASS smx-min-core` e la simulazione termina con
`EXIT SUCCESS`. Il programma confronta 246 casi (41 lunghezze, tre pattern,
due offset). I test `lit` coprono anche feature assente, RV64, load volatile,
minimo unsigned e loop con store. Non serve un nuovo opcode DWARF: l'istruzione
usa registri RISC-V esistenti e `-g` lega l'indirizzo alla riga C.

## Contratto del caso studio

```c
int8_t min_i8(const int8_t *a, size_t n) {
  int8_t min = 127;
  for (size_t i = 0; i < n; ++i)
    if (min > a[i]) min = a[i];
  return min;
}
```

`smx.contx` riceve due parole da quattro byte e aggiorna massimo, minimo e
shift nel contesto. Il pass estrae solo il minimo dai bit 15:8. Non
riconosce un calcolo di softmax completo e non cambia loop con effetti
collaterali o load volatile/atomici. Il risultato deve coincidere con il loop
scalare per ogni `n`, compreso zero, e per ogni valore signed INT8.
