# Documentación de implementación — syscalls `mmap`, `munmap` y `msync` en Selfie

Este documento explica cómo funciona la implementación de las tres syscalls
del Proyecto 2 en `selfie.c`, en el mismo orden en que aparecen en el código.
Cada fase indica qué hace esa parte del código y dónde vive (nombres exactos
de función/variable, para buscarlos con `grep`).

Cubre las **tres** syscalls del proyecto:
- **`mmap`** (Lista 1, fases M0–M8).
- **`munmap`** (Lista 2, fases U0–U3).
- **`msync`** (Lista 3, fases S0–S3).

---

## Fase M0 — Variables y constantes globales

- `SYSCALL_MMAP = 402`, `SYSCALL_MUNMAP = 403`, `SYSCALL_MSYNC = 404`: números
  de syscall nuevos, después de los que ya usaba Selfie (56, 63, 64, 93, 214,
  215, 216, 401).
- `MMAP_BASE = 0x40000000` (1GB): dirección virtual donde empieza la zona de
  mmap. Está por encima del heap y muy por debajo del stack (que empieza cerca
  de `HIGHESTVIRTUALADDRESS`, ~4GB), así que ninguno de los dos la invade.
- `lseek` se declara como función del host (no es una syscall emulada de
  Selfie), junto a `read`/`write`/`open`. Se usa para mover el cursor de
  lectura/escritura de un archivo antes de leerlo o escribirlo.
- `SEEK_SET = 0` es una constante propia (Selfie no incluye `<unistd.h>`),
  igual que hace con `O_RDONLY`/`O_CREAT_TRUNC_WRONLY`.
- `page_cache_head` es la cabeza de la lista del page cache; es una variable
  global (no vive dentro de ningún proceso) porque el cache se comparte entre
  todos los procesos.
- `MMAP_PROT_READ/WRITE/READWRITE` también se definen aquí (ver M2).

---

## Fase M1 — Extender el contexto del proceso

- `CONTEXTENTRIES` pasa de 38 a 40 entradas.
- Entrada 38 = `mappings`: puntero a la lista de mappings del proceso.
- Entrada 39 = `mmap_next`: siguiente dirección virtual libre para usar cuando
  el proceso llama `mmap` con `addr = 0`.
- Accesores `get_mappings`/`set_mappings`/`get_mmap_next`/`set_mmap_next`,
  junto a los demás `get_*`/`set_*` del contexto.
- `init_context` inicializa todo proceso nuevo con `mappings = 0` y
  `mmap_next = MMAP_BASE`. Si el proceso es en realidad un hijo creado por
  `fork`, `implement_fork` (Fase M7) sobreescribe estos dos campos después
  con los valores reales heredados del padre.

---

## Fase M2 — Estructura de una entrada de mapping

Cada mapping es un nodo con 6 campos: `next`, `vaddr`, `length`, `prot`, `fd`,
`offset`. Tiene sus accesores `get_mapping_*`/`set_mapping_*` y
`allocate_mapping()` para crear un nodo nuevo.

Los valores de `prot` tienen nombre:
```c
uint64_t MMAP_PROT_READ      = 0;
uint64_t MMAP_PROT_WRITE     = 1;
uint64_t MMAP_PROT_READWRITE = 2;
```
Se usan tanto al guardar el mapping como al chequear permisos (M6).

`find_mapping_for_address(context, vaddr)` recorre `context->mappings` y
devuelve el nodo cuyo rango `[vaddr_inicial, vaddr_inicial + length)` contiene
la dirección buscada, o `0` si ninguno aplica. Esta función es el puente entre
la lista de mappings y el resto del sistema: la usan `is_valid_segment_read`,
`is_valid_segment_write` y `copy_buffer`.

---

## Fase M3 — Page cache global y `get_or_load_cache_frame`

El page cache es una lista con 4 campos por entrada: `next`, `fd`,
`file_page_offset`, `frame`. Cada entrada dice qué cache frame (página física)
contiene el contenido de una página concreta de un archivo.

`get_or_load_cache_frame(fd, file_offset)` funciona así:
- **Si la página ya está en cache** (existe una entrada con el mismo `fd` y
  `file_offset`): devuelve directamente ese frame, sin tocar el archivo. Esto
  es lo que permite que padre e hijo (tras un `fork`) o dos mappings del mismo
  archivo terminen usando el mismo frame físico.
- **Si no está en cache**: reserva un frame nuevo con `palloc()` (la misma
  función que usa el resto de Selfie para reservar páginas físicas),
  posiciona el cursor del archivo con `lseek(fd, file_offset, SEEK_SET)`, lee
  `PAGESIZE` bytes con `read()`, y guarda la nueva entrada al frente de la
  lista `page_cache_head`.

Nota importante sobre el `fd`: en Selfie, el file descriptor es local a cada
proceso, así que dos procesos que abren el mismo archivo de forma
independiente pueden tener valores de `fd` distintos (o el mismo valor
apuntando a archivos distintos). Por eso el cache solo funciona de forma
confiable entre procesos emparentados por `fork`, que heredan el mismo `fd`
real del sistema operativo host (ver M7).

---

## Fase M4 — `emit_mmap` (compilador)

Registra `mmap` en la tabla de símbolos con 5 parámetros, carga `addr,
length, prot, fd, offset` en ese orden a los registros `REG_A0..REG_A4`, y
emite un `ecall` con `SYSCALL_MMAP`.

---

## Fase M5 — `implement_mmap` (emulador)

Sigue los 8 pasos del enunciado:

1. Lee los 5 argumentos desde los registros donde el compilador los dejó.
2. Redondea `length` hacia arriba al múltiplo de `PAGESIZE` más cercano.
3. Si `addr` llegó como `0`, usa la siguiente dirección libre del proceso
   (`get_mmap_next`).
4. Valida la región con `is_valid_mmap_region` (ver más abajo) antes de tocar
   la tabla de páginas. Si no es válida, retorna error sin modificar nada.
5. Por cada página del mapping: obtiene o carga su cache frame con
   `get_or_load_cache_frame`, y mapea la página virtual a ese frame con:
   ```c
   set_PTE_for_page(get_pt(context), page, frame);
   ```
   Se usa `set_PTE_for_page()` en vez de `map_page()` a propósito: `map_page`
   actualiza `lowest/highest_lo_page` y `lowest/highest_hi_page`, que es
   justo lo que usa `implement_fork` para decidir qué páginas copiar
   físicamente. Si se usara `map_page()`, un `fork` duplicaría físicamente
   las páginas del mmap en vez de compartir el frame entre padre e hijo.
6. Avanza `mmap_next` para que el siguiente `mmap` con `addr = 0` no choque
   con este mapping.
7. Registra el mapping al frente de `context->mappings`.
8. Devuelve la dirección inicial del mapping como resultado de la syscall.

`is_valid_mmap_region(context, addr, length)` valida, antes de mapear
cualquier página, que: `length` no sea cero, `addr` esté alineada a página,
`addr` no invada code/data/heap (esté por encima de `MMAP_BASE`), no haya
overflow al sumar `addr + length`, el rango entre dentro del espacio de
direcciones virtuales, no invada el stack actualmente en uso, y no se
solape con ningún mapping ya existente del proceso.

---

## Fase M6 — Verificador de segmentos (lectura/escritura)

`is_valid_segment_read()` y `is_valid_segment_write()` ya distinguían entre
direcciones de data, stack y heap. Se les agregó una cuarta categoría: si la
dirección no cae en ninguna de esas tres, se busca con
`find_mapping_for_address` si pertenece a un mapping mmap, y se valida el
permiso (`prot`) de ese mapping:

```c
// is_valid_segment_read
if (mapping != 0)
  if (get_mapping_prot(mapping) != MMAP_PROT_WRITE) {  // lectura permitida
    mmap_reads = mmap_reads + 1;
    return 1;
  }

// is_valid_segment_write
if (mapping != 0)
  if (get_mapping_prot(mapping) != MMAP_PROT_READ) {   // escritura permitida
    mmap_writes = mmap_writes + 1;
    return 1;
  }
```

Un mapping con `prot = READ` no acepta escrituras, uno con `prot = WRITE` no
acepta lecturas, y uno con `prot = READWRITE` acepta ambas.

Además se agregaron contadores `mmap_reads`/`mmap_writes`, que se imprimen en
el perfil de memoria (`print_register_memory_profile`), igual que
`data_reads`, `heap_reads`, etc.

Este mismo chequeo de `prot` también se aplica dentro de `copy_buffer`, que es
la función que usan las syscalls `read`/`write` del kernel emulado para
copiar datos entre el buffer del host y la memoria virtual del proceso. Así,
un `read()`/`write()` de Selfie tampoco puede saltarse el permiso de un
mapping, igual que no puede un `load`/`store` normal de la CPU.

**Probado con:** un mapping `prot = 0` (solo lectura) rechaza una escritura
con `segmentation fault` (`test4`).

---

## Fase M7 — Herencia de mappings en `fork`

Cuando un proceso hace `fork`, `implement_fork` hace lo siguiente además de
lo que ya hacía para copiar código/datos/pila:

1. `set_mmap_next(child, get_mmap_next(context))`: el hijo copia la próxima
   dirección libre real del padre (en vez de quedarse con el `MMAP_BASE` por
   defecto que puso `init_context`).
2. Por cada mapping del padre: crea un nodo equivalente en el hijo (mismos
   `vaddr/length/prot/fd/offset`) y lo agrega al frente de
   `context->mappings` del hijo.
3. Por cada página de ese mapping: lee el frame del padre con
   `get_frame_for_page(get_pt(context), page)` y escribe el **mismo** frame
   en la tabla del hijo con `set_PTE_for_page()` (la misma operación de bajo
   nivel de M5, no `map_page()`). Así el frame físico no se duplica: padre e
   hijo terminan compartiendo la misma página física del page cache.

**Probado con:** `test3_fork_comparte_frame.c` — el hijo escribe, el padre
espera y lee la misma dirección mapeada y ve el cambio (exit code 88),
confirmando que comparten el mismo frame físico.

---

## Fase M8 — Conectar al despachador

En `handle_system_call`:

```c
else if (a7 == SYSCALL_MMAP)
  implement_mmap(context);
```

---

# `munmap(addr)` — Lista 2

`munmap` deshace un mapping previamente creado. Solo recibe `addr` (no
`length`, porque ese dato ya quedó guardado en el mapping al crearlo con
`mmap`). No toca el archivo: si el proceso modificó la región y solo llama
`munmap` sin haber llamado `msync` antes, esos cambios se pierden — eso es
lo que pide el enunciado.

## Fase U0 — Número de syscall

`SYSCALL_MUNMAP = 403`, el primer número libre después de `SYSCALL_MMAP`.

---

## Fase U1 — `emit_munmap` (compilador)

Registra `munmap` en la tabla de símbolos con **1 solo parámetro** (`addr`),
carga ese argumento a `REG_A0`, y emite `ecall` con `SYSCALL_MUNMAP`. Se
invoca junto a `emit_mmap()` en `selfie_compile()`.

```c
void emit_munmap() {
  create_symbol_table_entry(GLOBAL_TABLE, string_copy("munmap"),
    0, PROCEDURE, UINT64_T, 1, code_size);

  emit_load(REG_A0, REG_SP, 0); // addr
  emit_addi(REG_SP, REG_SP, WORDSIZE);

  emit_addi(REG_A7, REG_ZR, SYSCALL_MUNMAP);

  emit_ecall();
  emit_jalr(REG_ZR, REG_RA, 0);
}
```

---

## Fase U2 — `implement_munmap` (emulador)

Sigue los 5 pasos del enunciado:

1. Lee `addr` desde `REG_A0`.
2. Recorre `context->mappings` buscando la entrada cuya `vaddr` coincide
   exactamente con `addr`, guardando el nodo anterior (`previous_mapping`)
   para poder desconectarlo después.
3. Si la encuentra: limpia la entrada de la tabla de páginas de **cada
   página** del mapping con la operación de bajo nivel:

   ```c
   set_PTE_for_page(get_pt(context), page, 0);
   ```

   `frame = 0` es exactamente el estado que `is_virtual_address_mapped()`/
   `get_frame_for_page()` interpretan como "página no mapeada". No se llama
   a `pfree()`: el frame físico no se libera, porque otro proceso (por
   ejemplo un hijo creado con `fork` antes de este `munmap`) puede seguir
   usándolo a través del page cache compartido.
4. Desconecta el nodo de `context->mappings` (si era el primero, la cabeza
   pasa a ser el siguiente; si no, se enlaza el anterior con el siguiente).
5. Retorna `0` en `REG_A0` si encontró y desmapeó, o `sign_shrink(-1,
   SYSCALL_BITWIDTH)` si `addr` no coincidía con ningún mapping registrado
   de ese proceso.

---

## Fase U3 — Conectar al despachador

En `handle_system_call`:

```c
else if (a7 == SYSCALL_MUNMAP)
  implement_munmap(context);
```

---

# `msync(addr)` — Lista 3

`msync` es el **único** punto del sistema que escribe cambios de una región
mapeada de vuelta al archivo en disco. Mientras no se llame, las
modificaciones quedan únicamente en el cache frame en memoria. Igual que
`munmap`, recibe solo `addr` (no `length`, porque ya está guardada en el
mapping).

## Fase S0 — Número de syscall y dependencias

`SYSCALL_MSYNC = 404`, junto a `SYSCALL_MMAP = 402` y `SYSCALL_MUNMAP = 403`.
La dependencia que necesita (`lseek`) ya está disponible desde la Fase M0.

---

## Fase S1 — `emit_msync` (compilador)

Mismo patrón que `munmap`: 1 solo parámetro (`addr`), lo carga a `REG_A0`, y
emite `ecall` con `SYSCALL_MSYNC`. Se invoca junto a
`emit_mmap()`/`emit_munmap()` en `selfie_compile()`.

```c
void emit_msync() {
  create_symbol_table_entry(GLOBAL_TABLE, string_copy("msync"),
    0, PROCEDURE, UINT64_T, 1, code_size);

  emit_load(REG_A0, REG_SP, 0); // addr
  emit_addi(REG_SP, REG_SP, WORDSIZE);

  emit_addi(REG_A7, REG_ZR, SYSCALL_MSYNC);

  emit_ecall();
  emit_jalr(REG_ZR, REG_RA, 0);
}
```

---

## Fase S2 — `implement_msync` (emulador)

1. Lee `addr` desde `REG_A0`.
2. Busca en `context->mappings` la entrada cuya `vaddr` coincide con `addr`
   (no hace falta desconectarla, a diferencia de `munmap`: el mapping sigue
   existiendo después de un `msync`).
3. Si no la encuentra: retorna error de inmediato (`sign_shrink(-1,
   SYSCALL_BITWIDTH)`).
4. Si la encuentra: extrae `fd = get_mapping_fd(mapping)`, `base_offset =
   get_mapping_offset(mapping)`, y `number_of_pages =
   get_mapping_length(mapping) / PAGESIZE`.
5. Por cada página: calcula el offset dentro del archivo (`base_offset +
   i*PAGESIZE`), obtiene el frame físico con `get_frame_for_page(get_pt
   (context), page)`, posiciona el cursor con `lseek(fd, file_offset,
   SEEK_SET)`, y escribe el frame completo al archivo:

   ```c
   write(fd, (uint64_t*) frame, PAGESIZE);
   ```

6. Retorna `0` en éxito.

**Por qué se usa `write()` del host directamente:** `implement_msync` corre
del lado del emulador, operando sobre una dirección **física** (el frame,
obtenida de la tabla de páginas), no sobre memoria virtual de ningún proceso
invitado. La syscall `write()` interna de Selfie (la que usan los programas
C* a través de `copy_buffer`) está diseñada para traducir direcciones
virtuales de un proceso; usarla aquí sería una traducción innecesaria, ya
que el frame **ya es** la dirección física que se necesita. Por eso `msync`
llama directamente a `write()`/`lseek()` del sistema operativo real, igual
que `get_or_load_cache_frame` (Fase M3) llama directamente a `read()` para
cargar una página por primera vez — son operaciones simétricas.

**Probado con:**
- `test9_msync_persiste_cambio_en_disco.c`: escribe en memoria, llama
  `msync`, y `od -c` confirma que el archivo en disco cambió exactamente a
  los bytes escritos.
- `test10_sin_msync_no_persiste.c` (control): la misma escritura, sin llamar
  `msync`, y el archivo en disco queda intacto.
- `test11_msync_addr_invalida_retorna_error.c`: `msync` sobre una dirección
  que no es el inicio de ningún mapping devuelve error sin crashear.

---

## Fase S3 — Conectar al despachador

En `handle_system_call`:

```c
else if (a7 == SYSCALL_MSYNC)
  implement_msync(context);
```

---

## Otros ajustes al resto del emulador

Para que mmap conviva con el resto de la memoria del proceso, se tocaron
además algunas funciones que ya existían en Selfie:

- **`is_stack_address`**: el rango de stack se calcula desde `SP` hasta
  `HIGHESTVIRTUALADDRESS`. Se le agregó el límite superior `mmap_next`, para
  que una dirección dentro de la zona de mmap no se confunda con stack (si
  no, un `SP` que baja hasta esa zona haría que `handle_page_fault` le
  asignara un frame nuevo, destruyendo el mapping).
- **`is_address_between_stack_and_heap`**: se le agregó el límite `MMAP_BASE`,
  para que el heap no pueda crecer dentro de la zona reservada para mmap.
- **`copy_buffer`** (usada por las syscalls `read`/`write` del kernel
  emulado): reconoce también direcciones dentro de un mapping mmap, además
  de data/stack/heap, y respeta el `prot` del mapping igual que lo hacen los
  `load`/`store` normales de la CPU.

---

## Tests entregados (verificados, exit code confirmado)

| Test | Qué valida | Resultado esperado |
|------|------------|---------------------|
| `test1_lectura_directa.c` | Lectura por CPU (`*addr`) de contenido mapeado | exit code **42** |
| `test2_escritura_y_lectura.c` | Escritura por CPU y lectura de vuelta (in-memory) | exit code **77** |
| `test3_fork_comparte_frame.c` | Padre e hijo comparten el mismo cache frame (M7) | exit code **88** |
| `test4_prot_solo_lectura_rechaza_escritura.c` | `prot=0` rechaza escritura (M6) | `segmentation fault` |
| `test5_dos_mmap_mismo_archivo_comparten_frame.c` | Dos `mmap` del mismo archivo/offset comparten frame (M3) | exit code **99** |
| `test6_munmap_acceso_posterior_falla.c` | Tras `munmap`, acceder a esa dirección falla | `segmentation fault` |
| `test7_munmap_addr_invalida_retorna_error.c` | `munmap` sobre dirección no registrada retorna error | exit code **33** |
| `test8_munmap_no_afecta_a_otro_proceso.c` | `munmap` del hijo no libera el frame compartido con el padre | exit code **66** |
| `test9_msync_persiste_cambio_en_disco.c` | `msync` escribe el cambio al archivo en disco | exit code **44** (+ archivo cambiado) |
| `test10_sin_msync_no_persiste.c` | Sin `msync`, el archivo en disco NO cambia (control) | archivo intacto |
| `test11_msync_addr_invalida_retorna_error.c` | `msync` sobre dirección no registrada retorna error | exit code **22** |

Con estos 11 tests quedan cubiertos los tres escenarios mínimos que pide el
enunciado: (1) crear un mapping y leer su contenido directamente de memoria,
(2) crear un mapping, escribir sobre él y mostrar el cambio en memoria
secundaria (vía `msync`), y (3) dos procesos que comparten un mapping del
mismo archivo y observan los cambios entre sí (vía `fork`).