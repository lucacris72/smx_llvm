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
5. **Estremi e allineamento**: lo step5 estende la pass a massimo e min/max
   insieme, seed arbitrari e inizializzazione da `a[0]`. I load packed sono
   allineati a quattro byte; un prefisso scalare gestisce i puntatori non
   allineati. [Implementazione e verifiche](step5-implementation.md).

Per ogni tappa: sorgente, comando, IR/assembly/ELF prima e dopo, test e una
breve spiegazione del livello LLVM modificato. I commit seguono lo stesso
ordine e non importano il vecchio pass sperimentale.

## Base riproducibile

Il ramo `main` parte dal commit `e8426c15b` di LLVM 18
e conserva l'RTL come submodule al commit `1d8dff15a190e3e14fa3d1513bc31d5247eee2b7`.
L'estensione dello step5 e' sul ramo `codex/smx-extrema-aligned`, creato da
`main`. `build/` e' locale e non versionato.

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

L'esempio introduttivo [`step3/ex1`](../examples/step3/ex1/readme.md) mostra
prima la sola funzione `min_i8`: C, IR prima/dopo il pass e assembly con/senza
Xsmx. Non richiede linking o simulazione:

```sh
make -C examples/step3/ex1 check
```

Per confrontare lo stesso loop con e senza Xsmx su RTL, inclusi cicli e speedup:

```sh
make -C examples/step3/ex2 check
```

L'esempio usa fino a 256 elementi, confronta 60 casi con un riferimento Python
e misura `mcycle` attorno a 16 chiamate. Il risultato e' documentato in
[`examples/step3/ex2/readme.md`](../examples/step3/ex2/readme.md).

L'esempio finale mantiene la softmax dello step0 per ispezionare IR prima/dopo
le ottimizzazioni e assembly builtin/volatile con mnemonici. Il benchmark RTL
misura soltanto SMX compilato con LLVM, confrontandolo con FP32, software Int8
e SMX storici della repo hardware, conservati in un CSV immutato:

```sh
make -s -C examples/step4
make -s -C examples/step4 run
```

[`examples/step4/readme.md`](../examples/step4/readme.md) documenta gli
artefatti e i risultati. `make -C examples/step0 ir` genera il riferimento
LLVM con assembly volatile per il confronto manuale. `N=63` resta il default
solo per IR/assembly; lo sweep usa sempre le otto dimensioni storiche 8..1024,
compila a `-O3` e misura `mcycle64`. Python verifica ogni parola col modello
LUT; FP32/software non sono rieseguiti. Risultati e grafici PNG/SVG (X log,
X lineare e solo SMX/LLVM) sono versionati in `examples/step4/results/`.
Il confronto storico non isola l'effetto dei builtin dal cambio di compilatore
e harness. Non richiede altre modifiche a LLVM o all'hardware.

## Come le modifiche diventano tool utilizzabili

I sorgenti modificati sono in `llvm-project/`, mentre i programmi che
eseguiamo sono in `build/llvm-course/bin/`. Le modifiche non vengono lette a
runtime: sono integrate nei tool attraverso generazione di codice,
compilazione C++ e linking. Non carichiamo una pass tramite plugin e non
sostituiamo il Clang di sistema.

| Tappa | Integrazione nella build | Tool interessati |
| --- | --- | --- |
| ISA/MC | TableGen legge i `.td` RISC-V e genera tabelle di istruzioni, matcher, encoder, printer e decoder; il codice C++ registra anche `xsmx` nel parser ISA | `llvm-mc`, `llvm-objdump`, `llc`, Clang e altri utenti del backend |
| Builtin/intrinsic | Le definizioni builtin e il lowering C++ entrano nel frontend Clang; TableGen genera gli intrinsic LLVM e i pattern di selezione RISC-V | Clang, `opt`, `llc` |
| Pass automatica | `SMXMinLoop.cpp` entra in `LLVMScalarOpts` tramite CMake; `PassRegistry.def` espone `smx-min-idiom` a `opt`; il target RISC-V inserisce il pass nella pipeline a `-O2`/`-O3` | `opt`, Clang |

Nella build locale queste librerie LLVM sono linkate staticamente nei tool.
Il codice macchina emesso per il firmware e' invece RISC-V: costruiamo un
compilatore che gira sulla macchina host e genera codice per il target, non
un compilatore da eseguire sul core SMX.

La configurazione CMake completa e' nella sezione "Base riproducibile".
Dopo una modifica ai sorgenti LLVM/Clang, si aggiorna la build esistente con:

```sh
ninja -C build/llvm-course -j4 clang opt llc llvm-mc llvm-objdump \
  llvm-readelf llvm-dwarfdump FileCheck not count llvm-config
make check-llvm
```

Ninja rigenera i file TableGen, ricompila i sorgenti coinvolti e rilinka i
tool dipendenti quando necessario; non serve ripartire da una build pulita.
Se cambiano `CMakeLists.txt` o le opzioni della configurazione, CMake deve
rigenerare la build (Ninja lo invoca automaticamente per le modifiche CMake
tracciate; per cambiare opzioni si rilancia il comando `cmake`). I file
generati sotto `build/` non si modificano a mano.

I Makefile degli esempi selezionano esplicitamente la build locale attraverso
`LLVM_TOOLS`, evitando di usare per errore un Clang vanilla. L'ISA si abilita
con `-march=rv32i_xsmx` o con le altre estensioni del core piu' `_xsmx`.
Negli esempi che usano i wrapper, `SMX_USE_LLVM_INTRINSICS=1` sceglie i builtin.
Per i loop automatici non serve quel define: il pass controlla RV32 e la
feature Xsmx della funzione.

Lo step3 segue questa catena:

```text
C: min_i8 -> IR scalare con llvm.smin.i8
         -> SMXMinLoopPass: due load packed + llvm.riscv.smx.contx + coda scalare
         -> selezione istruzioni RISC-V -> smx.contx -> encoding nell'oggetto
```

`opt -passes='loop-simplify,lcssa,smx-min-idiom'` rende visibile il pass
isolatamente. Clang lo esegue automaticamente a `-O2`/`-O3` tramite il callback
in `RISCVTargetMachine.cpp`; a `-O1` l'esempio conserva l'IR prima del pass.
E' un pass dedicato, non una modifica a `LoopIdiomRecognize.cpp`.

Infine, una modifica ai soli esempi C, ai Makefile, ai test o ai README non
richiede di ricompilare LLVM. Basta ricompilare gli esempi o rilanciare i test.
Gli ELF di simulazione usano il linker e il runtime GNU gia' esistenti: non
serve ricostruire newlib o modificare il linking per ogni nuova istruzione.

I Makefile di `step3/ex1`, `step4` e `step5` tracciano anche i binari dei tool come
dipendenze degli artefatti di compilazione, quindi una loro ricompilazione
rigenera l'IR/assembly dell'esempio. Per gli
altri esempi, i cui Makefile non tracciano ancora queste dipendenze, dopo aver
aggiornato i tool usare `make -C examples/<step> clean` e poi `make ... check`
per evitare di leggere artefatti generati dal compilatore precedente.

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
shift nel contesto. Il caso introduttivo estrae il minimo dai bit 15:8;
lo step5 usa anche il massimo nei bit 7:0 e fonde le due riduzioni. Non
riconosce un calcolo di softmax completo e non cambia loop con effetti
collaterali o load volatile/atomici. Il risultato deve coincidere con il loop
scalare per ogni `n`, compreso zero, e per ogni valore signed INT8.
