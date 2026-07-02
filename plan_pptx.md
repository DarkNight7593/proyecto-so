# Plan de diseño — PPTX Proyecto 2: Memory Mappings en Selfie
### 22 slides, flujo de comprensión (base → estructuras → mmap → munmap → msync → fork → cierre)

Convenciones de este plan:
- **[TÍTULO]** = título de la slide.
- **Texto para la slide** = lo que va escrito/leído (tú lo puedes acortar en bullets).
- **Diagrama** = ASCII art recreando el estilo de tus imágenes de referencia (monoespaciado, cajas y flechas), pensado para pasar a shapes/textboxes en pptxgenjs con tema oscuro.
- **Código** = cita exacta de `selfie.c` con número de línea aproximado (verificado contra el archivo que subiste, con `// #implementacion` marcando cada bloque tuyo), para que armes el code walkthrough.
- Uso `Núcleo A / B / C` donde corresponde, siguiendo lo que ya tenías anotado (page cache por fd+offset, set_PTE_for_page para compartir en fork, orden de validación).

Cambios de esta revisión respecto a la anterior:
- Se eliminó la slide dedicada solo a `lseek`: ahora se menciona en 2-3 líneas dentro de la slide 3, junto a los números de syscall.
- Se rehizo el diagrama de la slide 2: la versión anterior tenía una flecha "proceso -- read/write --> proceso" que confundía (parecía que un proceso se transformaba en otro). El nuevo diagrama deja claro que `read`/`write` mueven bytes entre el archivo y un buffer *dentro* del mismo proceso, y que `mmap` conecta memoria y archivo directamente.
- Pasé slide por slide revisando contra `selfie.c` de nuevo: agregué una aclaración sobre dónde vive físicamente cada estructura nueva (host vs. memoria emulada del proceso), until ahora implícita, y afiné varios textos para que se lean mejor en voz alta.
- Renumeración completa: todo lo que antes era slide 5 en adelante bajó un número.

---

## SLIDE 1 — Portada

**[Memory Mappings en Selfie: mmap, munmap y msync]**

Texto para la slide:
- CS3015 Sistemas Operativos — Proyecto 2
- Tirsa · Jorge Gonzalez · Mauricio Pinto
- TA: Mariana Capuñay
- Junio 2026

Diagrama: ninguno, solo logo/portada con el esquema de color oscuro que ya vienes usando.

---

## SLIDE 2 — El problema: ¿qué es un memory mapping?

**[¿Qué resuelve mmap?]**

Texto para la slide:
- Sin mmap, un proceso que quiere leer o escribir un archivo tiene que pasar siempre por las syscalls `read`/`write`: cada llamada copia bytes entre el archivo (en disco) y un buffer que vive dentro de la memoria del propio proceso. Dos copias, nunca acceso directo.
- Con mmap, el archivo se "proyecta" sobre un rango de direcciones virtuales del proceso. A partir de ahí, leer o escribir esa memoria **es** leer o escribir el archivo — sin pasar por `read`/`write`, y sin que el contenido llegue a disco hasta que se llama `msync`.
- Beneficio clave del proyecto: **varios procesos pueden compartir la misma página física** de un archivo sin duplicarla. Lo vas a ver en detalle más adelante: cuando un proceso hace `fork`, el hijo hereda el mismo mapping y termina apuntando exactamente al mismo cache frame que el padre — no se copia nada.

Diagrama (rehecho — la versión anterior con "proceso --read/write--> proceso" no dejaba claro qué pasaba; ahora se ve que todo el movimiento de datos ocurre entre el archivo y un buffer *dentro* del proceso):

```
   SIN mmap: dos copias por cada acceso           CON mmap: una sola conexión, acceso directo

   proceso                                        proceso
   ┌────────────────────┐                         ┌────────────────────┐
   │                     │                         │                     │
   │   buffer (memoria   │   read(fd,buf,n)        │   región mapeada    │
   │   normal del        │◄────────────────┐       │   (addr, length)    │
   │   proceso)          │                 │       │   ███████████████   │◄──┐
   │                     │   write(fd,buf,n)│       │                     │   │ *addr = valor
   │                     ├────────────────┐ │       │                     │   │  (load/store
   └──────────┬──────────┘                │ │       └──────────┬──────────┘   │   directo, sin
              │                           │ │                  │              │   llamar a nada)
              │ cada read/write           │ │                  │
              │ mueve bytes                │ │                  │ mmap(addr,length,
              ▼ entre archivo y buffer     ▼ │                  │ prot,fd,offset)
   ┌─────────────────────────────────────────┐                 ▼ conecta memoria y archivo
   │                  archivo                │       ┌─────────────────────────────┐
   │                 (disco)                 │◄──────┤          archivo             │
   └──────────────────────────────────────────┘ msync │         (disco)              │
                                              únicamente└─────────────────────────────┘
                                              cuando se llama msync()
```

Código: no aplica todavía (slide conceptual).

---

## SLIDE 3 — Las tres syscalls que se implementaron

**[Las 3 piezas: mmap · munmap · msync]**

Texto para la slide:
- `mmap(addr, length, prot, fd, offset)` → crea el mapping.
- `munmap(addr)` → lo destruye (sin escribir al archivo).
- `msync(addr)` → el único punto que persiste cambios al archivo.
- Los tres números de syscall son nuevos, siguen la secuencia de Selfie (la última syscall existente era 401).
- Detalle chico pero importante: para que `mmap`/`msync` puedan posicionar el cursor de un archivo en un offset arbitrario, se agregó `lseek` a la lista de funciones que Selfie ya "tomaba prestadas" del sistema operativo real (junto a `read`, `write`, `open`, que también están declaradas así, sin implementación en C*, conectadas directo a la syscall del host). No fue necesario escribir su lógica, solo declararla igual que las demás.

Código:
```c
// selfie.c ~1369-1375
// #implementacion mmap/munmap/msync: declaraciones de las tres syscalls
uint64_t SYSCALL_MMAP   = 402;
uint64_t SYSCALL_MUNMAP = 403;
uint64_t SYSCALL_MSYNC  = 404;

// selfie.c ~100-104
uint64_t read(uint64_t fd, uint64_t* buffer, uint64_t bytes_to_read);
uint64_t write(uint64_t fd, uint64_t* buffer, uint64_t bytes_to_write);
// #implementacion mmap: lseek es funcion del host que usan mmap y msync
// para posicionar el cursor del archivo
uint64_t lseek(uint64_t fd, uint64_t offset, uint64_t whence);
```

Diagrama: tabla simple (no ASCII-art, tabla de pptx) con columnas: Syscall | Parámetros | Qué hace | Toca el archivo. Puedes agregar una fila chica al pie: "lseek: función del host, sin número de syscall propio — la usan mmap/msync internamente".

---

## SLIDE 4 — Dónde vive mmap en el espacio de memoria (recreación de tu imagen 2)

**[Ubicando la zona de mmap en el mapa de memoria]**

Texto para la slide:
- Selfie ya dividía el espacio virtual en STACK (arriba, crece hacia abajo), HEAP (crece hacia arriba), DATA y CODE (abajo).
- Se agregó una nueva zona, `MMAP_BASE = 0x40000000` (1GB), por encima de donde termina el heap típico y muy por debajo de donde arranca el stack (~4GB).
- Esto evita dos colisiones: que el heap crezca "hacia adentro" de mmap, y que el stack baje y pise una región mapeada. Ambos chequeos se agregaron a las funciones que Selfie ya usaba para clasificar direcciones — no se creó un mecanismo nuevo, se extendió el existente.

Diagrama (recreación de tu imagen 2, agregando la franja MMAP):

```
        4GB ┌─────────────┐
            │    STACK    │  ← crece hacia abajo (SP)
            │      ↓      │
            ├─────────────┤
            │             │
            │   (libre)   │
            │             │
            ├─────────────┤
 MMAP_BASE →│  MMAP ZONE  │  ← mappings de mmap() viven aquí
 0x40000000 │      ↑      │     (mmap_next avanza hacia arriba)
            ├─────────────┤
            │      ↑      │
            │    HEAP     │  ← crece hacia arriba (malloc)
            ├╌╌╌╌╌╌╌╌╌╌╌╌╌┤
            │    DATA     │  ← globales, strings, big ints
            ├─────────────┤
            │    CODE     │  ← instrucciones de máquina
          0 └─────────────┘
```

Código:
```c
// selfie.c ~1378
// #implementacion mmap: direccion base (1GB) usada cuando addr=0
uint64_t MMAP_BASE = 1073741824; // = 0x40000000

// selfie.c ~12212-12221 (is_address_between_stack_and_heap)
if (vaddr >= get_program_break(context))
  // #implementacion mmap: el heap no puede crecer dentro de la zona
  // reservada para mmap (a partir de MMAP_BASE)
  if (vaddr < MMAP_BASE)
    if (vaddr < *(get_regs(context) + REG_SP))
      return 1;

// selfie.c ~12192-12201 (is_stack_address)
if (vaddr >= *(get_regs(context) + REG_SP))
  if (vaddr <= HIGHESTVIRTUALADDRESS)
    // #implementacion mmap: excluye la zona de mmap del rango de stack
    if (vaddr >= get_mmap_next(context))
      return 1;
```

---

## SLIDE 5 — Vista panorámica del flujo completo (recreación de tu imagen 1)

**[El mapa completo: de la dirección virtual al archivo]**

Texto para la slide:
- Esta es la idea central de todo el proyecto, la vas a ver repetida en detalle en las próximas slides.
- Cuatro piezas nuevas conectan al proceso con el archivo: **tabla de páginas** (ya existía, se reutiliza), **cache frames** (páginas físicas nuevas), **page cache entries** (el "índice" que dice qué frame tiene qué página del archivo), y el **archivo** mismo.
- `mmap` arma esta cadena completa. `msync` es la única flecha que va del cache frame de vuelta al archivo.
- Dato importante que vas a repetir en varias slides: el índice del page cache se arma con la pareja **(fd, offset)**, no con un identificador único del archivo (como un inodo). Esto funciona perfecto para compartir memoria entre un proceso y sus hijos de `fork` (heredan el mismo fd real del host), pero dos procesos que abren el mismo archivo de forma independiente podrían tener `fd` distintos y no compartirían el cache frame.

Diagrama (recreación fiel de tu imagen 1):

```
 Espacio de direcciones      Tabla de       Cache frames        Page cache          Archivo
 virtual del proceso         páginas        en memoria física   entries
 ┌───────────┐ 0x0000       ┌─────┐         ┌─────────────┐     ┌──────────────────┐   ┌──────────────┐
 │           │              │  :  │         │             │     │                  │   │              │
 │   . . .   │              ├─────┤         ├─────────────┤     ├──────────────────┤   ├──────────────┤
 │███████████│◄── mmap ────►│  •──┼────────►│  frame F_0  │◄────│(fd, 0)    -> F_0  │◄╌╌│file page 0   │
 │███████████│  (addr,      ├─────┤         ├─────────────┤     ├──────────────────┤   │offset 0      │
 │███████████│   length)    │  •──┼────────►│  frame F_1  │◄────│(fd, 4096) -> F_1  │◄╌╌│file page 1   │
 │           │              ├─────┤         ├─────────────┤     ├──────────────────┤   │offset 4096   │
 │   . . .   │              │  •──┼────────►│  frame F_2  │◄────│(fd, 8192) -> F_2  │◄╌╌│file page 2   │
 │           │              ├─────┤         ├─────────────┤     ├──────────────────┤   │offset 8192   │
 │           │ 0xFFFF       │  :  │         │      :      │     │        :         │   │      :       │
 └───────────┘              └─────┘         └─────────────┘     └──────────────────┘   └──────────────┘

 mmap crea entradas en context->mappings y llama set_PTE_for_page() para que las
 direcciones virtuales apunten directo al cache frame. El page cache encuentra ese
 frame usando (fd, offset) — no un id de archivo global. msync es la única flecha
 que escribe de vuelta al archivo.
```

Código: ninguno todavía, esta slide es el "mapa" que ancla el resto de la presentación.

---

## SLIDE 6 — El contexto del proceso: dos campos nuevos

**[Cómo Selfie recuerda los mappings de cada proceso]**

Texto para la slide:
- Cada proceso en Selfie es un `context`, un arreglo de `uint64_t` de tamaño fijo. Antes tenía 38 entradas, ahora tiene 40.
- Entrada 38 (`mappings`): puntero a la lista de mappings de ese proceso (uno por cada `mmap` que hizo y no ha desmapeado).
- Entrada 39 (`mmap_next`): la siguiente dirección virtual libre para usar cuando el proceso llama `mmap(0, ...)`.
- Todo proceso nuevo arranca sin mappings y con `mmap_next = MMAP_BASE`. Si es un hijo de `fork`, estos dos campos se sobreescriben después con los del padre (slides 18-19).

Diagrama:

```
context (arreglo de 40 uint64_t)
 ┌────┬───────────────────┬─────────────────────────────────────┐
 │ .. │        ...        │  ... (pc, registros, page table,     │
 │    │                   │       pid, estado, hijos, etc.)      │
 ├────┼───────────────────┼─────────────────────────────────────┤
 │ 38 │     mappings   ───┼──► lista enlazada de mappings ──►... │
 ├────┼───────────────────┼─────────────────────────────────────┤
 │ 39 │     mmap_next      │  próxima dirección libre (>=MMAP_BASE)│
 └────┴───────────────────┴─────────────────────────────────────┘
```

Código:
```c
// selfie.c ~2320-2322
// #implementacion mmap: entradas 38 y 39 del contexto usadas por mmap
// | 38 | mappings         | pointer to head of this process' mmap mapping list
// | 39 | mmap next        | next free virtual address for mmap when addr == 0

// selfie.c ~2328
// #implementacion mmap: contexto extendido a 40 entradas
uint64_t CONTEXTENTRIES = 40;

// selfie.c ~2428 y ~2473
uint64_t* get_mappings(uint64_t* context)  { return (uint64_t*) *(context + 38); }
uint64_t  get_mmap_next(uint64_t* context) { return             *(context + 39); }

// selfie.c ~11705 (init_context)
// #implementacion mmap: todo proceso nuevo inicia sin mappings y con
// mmap_next en MMAP_BASE; implement_fork sobreescribe esto en hijos
set_mappings(context, (uint64_t*) 0);
set_mmap_next(context, MMAP_BASE);
```

---

## SLIDE 7 — Un mapping individual: los 6 campos

**[¿Qué guarda un mapping?]**

Texto para la slide:
- Un mapping es un nodo de una lista enlazada simple (uno por cada `mmap` que hizo el proceso, sin desmapear). El más nuevo se agrega al frente de la lista.
- 6 campos: puntero al siguiente nodo, dirección virtual inicial, tamaño, permisos, `fd` y offset dentro del archivo.
- `prot` tiene tres valores posibles, iguales en espíritu a los que usa la syscall real de Linux (`PROT_READ`/`PROT_WRITE`) pero simplificados a un solo campo con 3 casos.
- Detalle para si te preguntan: este nodo (y las entradas del page cache que ves en la próxima sección) se reserva con `smalloc()`, que es la función que usa el propio Selfie para su memoria interna — no es memoria virtual del proceso emulado (esa se pide con `palloc()`, como en la slide 10). Son dos "memorias" distintas: la del programa que Selfie está ejecutando, y la que usa Selfie mismo para llevar sus estructuras de control.

Diagrama:

```
 mapping (nodo, 6 campos, ver enunciado seccion 4 paso 1)
 ┌───┬──────────────────────────────────────┐
 │ 0 │ next        → siguiente mapping       │
 │ 1 │ vaddr       → dirección virtual inicial│
 │ 2 │ length      → tamaño en bytes (múlt. PAGESIZE)
 │ 3 │ prot        → permisos (ver árbol abajo)
 │ 4 │ fd          → file descriptor (local al proceso)
 │ 5 │ offset      → offset inicial dentro del archivo
 └───┴──────────────────────────────────────┘

     context->mappings
            │
            ▼
     ┌───────────┐     ┌───────────┐     ┌───────────┐
     │ mapping A │────►│ mapping B │────►│ mapping C │────► NULL
     └───────────┘     └───────────┘     └───────────┘
     (el más nuevo va al frente de la lista)
```

Árbol de decisión — permisos (`prot`):

```
                    prot recibido en mmap()
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                    │
   prot == 0           prot == 1            prot == 2
        │                   │                    │
  MMAP_PROT_READ     MMAP_PROT_WRITE      MMAP_PROT_READWRITE
  solo lectura        solo escritura       lectura y escritura
```

Código:
```c
// selfie.c ~11817-11821
// #implementacion mmap: MEMORY MAPPINGS, estructura y accesores
uint64_t MAPPINGENTRIES = 6;
uint64_t* allocate_mapping() { return smalloc(MAPPINGENTRIES * sizeof(uint64_t)); }

// selfie.c ~1381-1384
// #implementacion mmap: valores de prot de un mapping
uint64_t MMAP_PROT_READ      = 0;
uint64_t MMAP_PROT_WRITE     = 1;
uint64_t MMAP_PROT_READWRITE = 2;
```

---

## SLIDE 8 — Buscar un mapping por dirección: `find_mapping_for_address`

**[¿Esta dirección cae dentro de algún mapping?]**

Texto para la slide:
- Esta función es el puente entre "tengo una dirección virtual" y "sé a qué mapping pertenece".
- Se usa en 3 lugares distintos de Selfie: al validar lecturas (`is_valid_segment_read`), al validar escrituras (`is_valid_segment_write`), y dentro de `copy_buffer` (syscalls `read`/`write` del kernel emulado). La vas a ver repetida en las slides 16 y 17.
- Recorre la lista y devuelve el nodo cuyo rango `[vaddr, vaddr+length)` contiene la dirección buscada; si no encuentra nada, devuelve 0.

Diagrama:

```
  vaddr = 0x40001500
       │
       ▼
  ┌───────────────────────────────────────────────┐
  │ ¿vaddr >= mapping.vaddr  Y  vaddr < mapping.vaddr + mapping.length ?
  └───────────────────────────────────────────────┘
       │                                    │
      sí                                   no
       │                                    │
  devuelve ese mapping           avanza al siguiente mapping
                                  (si no hay más → devuelve 0)
```

Código:
```c
// selfie.c ~11837-11852
// #implementacion mmap: busca el mapping cuyo rango contiene vaddr,
// usado por is_valid_segment_read/write y copy_buffer
uint64_t* find_mapping_for_address(uint64_t* context, uint64_t vaddr) {
  uint64_t* mapping;
  mapping = get_mappings(context);
  while (mapping != (uint64_t*) 0) {
    if (vaddr >= get_mapping_vaddr(mapping))
      if (vaddr < get_mapping_vaddr(mapping) + get_mapping_length(mapping))
        return mapping;
    mapping = get_next_mapping(mapping);
  }
  return (uint64_t*) 0;
}
```

---

## SLIDE 9 — Núcleo A: el page cache global

**[El "índice" que comparten todos los procesos]**

Texto para la slide:
- A diferencia de `mappings` (que vive por-proceso dentro del context), el page cache es **una sola lista global**, compartida por todos los procesos del sistema — vive en una variable global, no dentro de ningún `context`.
- Cada entrada dice: "la página del archivo `fd` en el offset `X` está en el cache frame `F`".
- Esta es la pieza que permite que dos procesos —normalmente un padre y su hijo— que mapean el mismo archivo en el mismo offset terminen usando **la misma página física**, sin duplicar datos.
- Importante (repetido de la slide 5): la clave es `(fd, offset)`, **no un identificador de archivo como el inodo**. La garantía de compartir memoria real solo está probada y es confiable entre procesos emparentados por `fork`, porque heredan literalmente el mismo `fd` del host. Dos procesos que abren el mismo archivo de forma independiente (`open` cada uno por su cuenta) podrían recibir `fd` distintos del host y terminar con cache frames separados para el mismo contenido.

Diagrama:

```
                    page_cache_head (variable global,
                    NO vive dentro de ningún context)
                            │
                            ▼
        ┌───────────────────────┐   ┌───────────────────────┐
        │ fd=23  offset=0       │   │ fd=23  offset=4096     │
        │ frame → F_0           │──►│ frame → F_1            │──► ... ► NULL
        └───────────────────────┘   └───────────────────────┘
              ▲                             ▲
              │                             │
        proceso padre                 proceso hijo (tras fork)
        mapea fd=23 offset=0          mapea fd=23 offset=0
              └─────────── mismo frame F_0 ───────────┘
                 (mismo fd porque el hijo lo heredó
                  del padre, no porque abrió el archivo)
```

Código:
```c
// selfie.c ~1402
// #implementacion mmap: cabeza del page cache global, compartida por
// todos los procesos para que compartan frames del mismo archivo/offset
uint64_t* page_cache_head = (uint64_t*) 0;

// selfie.c ~11894-11901
// #implementacion mmap: PAGE CACHE, estructura y accesores
// | 0 | next entry       | siguiente entrada del cache
// | 1 | file descriptor  | fd del host (limitacion: fd es local al proceso,
// |   |                  | solo confiable entre procesos emparentados via fork)
// | 2 | file page offset | offset (multiplo de PAGESIZE) de la pagina cacheada
// | 3 | frame            | direccion fisica del cache frame con esos datos
uint64_t CACHEENTRYENTRIES = 4;
```

---

## SLIDE 10 — Núcleo A: `get_or_load_cache_frame` (cache hit vs miss)

**[¿Ya tengo esta página en memoria, o tengo que leerla del disco?]**

Texto para la slide:
- Esta función es el corazón del page cache. La llama `implement_mmap` una vez por cada página del mapping.
- **Hit**: la página ya está cacheada (algún proceso ya la mapeó antes, con el mismo `fd`/offset) → se reutiliza el frame, no se toca el archivo.
- **Miss**: primera vez que se pide esa página → se reserva un frame nuevo con `palloc()` (la misma función que usa el resto de Selfie para reservar páginas físicas de memoria emulada, distinta del `smalloc()` de la slide 7), se posiciona el archivo con `lseek`, y se lee con `read()`.
- Cada entrada nueva se agrega al **frente** de la lista global, igual que hacen los mappings dentro de cada proceso.

Diagrama (árbol de decisión):

```
        get_or_load_cache_frame(fd, file_offset)
                        │
                        ▼
        ¿existe una entrada en page_cache_head
         con el mismo (fd, file_offset)?
                        │
          ┌─────────────┴─────────────┐
         SÍ (hit)                    NO (miss)
          │                             │
   devuelve el frame          frame = palloc()
   ya guardado, sin           lseek(fd, file_offset, SEEK_SET)
   tocar el archivo           read(fd, frame, PAGESIZE)
                              se crea una entrada nueva
                              y se agrega al frente de la lista
                                     │
                                     ▼
                              devuelve el frame nuevo
```

Código:
```c
// selfie.c ~11919-11963
// #implementacion mmap: devuelve el frame cacheado para (fd, offset) si
// existe, o reserva uno con palloc() y carga la pagina del archivo si no
uint64_t get_or_load_cache_frame(uint64_t fd, uint64_t file_offset) {
  entry = page_cache_head;
  while (entry != (uint64_t*) 0) {
    if (get_cache_entry_fd(entry) == fd)
      if (get_cache_entry_offset(entry) == file_offset)
        return get_cache_entry_frame(entry);      // HIT
    entry = get_next_cache_entry(entry);
  }
  // MISS
  frame = palloc();
  lseek(fd, file_offset, SEEK_SET);
  bytes_read = read(fd, frame, PAGESIZE);
  entry = allocate_cache_entry();
  set_cache_entry_fd(entry, fd);
  set_cache_entry_offset(entry, file_offset);
  set_cache_entry_frame(entry, (uint64_t) frame);
  set_next_cache_entry(entry, page_cache_head);
  page_cache_head = entry;
  return (uint64_t) frame;
}
```

Nota de producción: activando `debug_mmap` (flag ya existente en el archivo), cada hit/miss imprime una línea (`"page cache hit for fd ... reusing frame ..."` / `"page cache miss ... read N bytes into frame ..."`) — útil para la demo en vivo si quieres mostrarlo en consola.

---

## SLIDE 11 — Núcleo C: validar la región antes de mapear

**[Antes de tocar la tabla de páginas: ¿esta región es válida?]**

Texto para la slide:
- `set_PTE_for_page` (la función de bajo nivel que usa `implement_mmap`) **no chequea límites** — si le pasas basura, escribe basura.
- Por eso `is_valid_mmap_region` corre primero y rechaza cualquier región problemática, en un orden pensado para cortar temprano los casos más baratos de detectar. Incluye, entre otras cosas, rechazar un `length == 0` (un mapping vacío no tiene sentido y podría dejar estructuras a medio crear).
- El chequeo de overlap contra mappings existentes se escribe con `if` anidados en vez de `&&`, porque **C* (el subconjunto de C que compila Selfie) no soporta operadores lógicos `||`/`&&`** — cada condición compuesta se arma con ifs anidados o secuenciales, como vas a ver repetido en varias funciones de este proyecto.

Diagrama (árbol de decisión, el "Núcleo C" — orden de validación):

```
        is_valid_mmap_region(context, addr, length)
                        │
                        ▼
              length == 0? ───── sí ──► INVÁLIDO (mapping vacío)
                        │ no
                        ▼
        addr % PAGESIZE != 0? ── sí ──► INVÁLIDO (no alineado)
                        │ no
                        ▼
          addr < MMAP_BASE? ──── sí ──► INVÁLIDO (invade code/data/heap)
                        │ no
                        ▼
     addr + length < addr? ───── sí ──► INVÁLIDO (overflow de 64 bits)
                        │ no
                        ▼
  addr+length-WORD > HIGHEST     sí ──► INVÁLIDO (fuera del espacio virtual)
  VIRTUALADDRESS?
                        │ no
                        ▼
   addr + length > SP actual? ── sí ──► INVÁLIDO (invade el stack en uso)
                        │ no
                        ▼
   ¿se solapa con algún mapping  sí ──► INVÁLIDO (overlap, chequeado con
   ya existente del proceso?          ifs anidados: C* no tiene &&/||)
                        │ no
                        ▼
                     VÁLIDO
```

Código:
```c
// selfie.c ~11854-11888
// #implementacion mmap: valida que addr y length sean una region valida
// (alineada, dentro del espacio virtual, sin overlap) antes de mapear
uint64_t is_valid_mmap_region(uint64_t* context, uint64_t addr, uint64_t length) {
  if (length == 0) return 0;                 // mapping vacio, no tiene sentido
  if (addr % PAGESIZE != 0) return 0;         // debe alinear a pagina
  if (addr < MMAP_BASE) return 0;             // no invade code/data/heap
  if (addr + length < addr) return 0;         // overflow de addr + length
  if (addr + length - WORDSIZE > HIGHESTVIRTUALADDRESS) return 0;
  if (addr + length > *(get_regs(context) + REG_SP)) return 0; // invade stack

  mapping = get_mappings(context);
  while (mapping != (uint64_t*) 0) {
    // se solapa si los rangos [addr,addr+length) y [vaddr,vaddr+len) se cruzan
    if (addr < get_mapping_vaddr(mapping) + get_mapping_length(mapping))
      if (addr + length > get_mapping_vaddr(mapping))
        return 0;
    mapping = get_next_mapping(mapping);
  }
  return 1;
}
```

---

## SLIDE 12 — `mmap`: el lado del compilador (`emit_mmap`)

**[De C* a RISC-V: cómo se emite la llamada]**

Texto para la slide:
- `mmap` recibe 5 parámetros. El compilador de Selfie los toma de la pila y los coloca en los registros de argumento `A0..A4`, en ese orden.
- Luego carga el número de syscall en `A7` y emite la instrucción `ecall`, que dispara el trap hacia el emulador. `emit_munmap`/`emit_msync` siguen exactamente el mismo patrón, solo que con un único parámetro (`addr` en `A0`).

Diagrama:

```
   pila (antes del ecall)             registros
   ┌────────────┐                     ┌────┬─────────┐
   │  offset    │──emit_load─────────►│ A4 │ offset  │
   ├────────────┤                     ├────┼─────────┤
   │  fd        │──emit_load─────────►│ A3 │ fd      │
   ├────────────┤                     ├────┼─────────┤
   │  prot      │──emit_load─────────►│ A2 │ prot    │
   ├────────────┤                     ├────┼─────────┤
   │  length    │──emit_load─────────►│ A1 │ length  │
   ├────────────┤                     ├────┼─────────┤
   │  addr      │──emit_load─────────►│ A0 │ addr    │
   └────────────┘                     ├────┼─────────┤
                                       │ A7 │ 402     │──► ecall
                                       └────┴─────────┘
```

Código:
```c
// selfie.c ~8350-8362 y siguientes
// #implementacion mmap: carga los 5 parametros (addr,length,prot,fd,offset)
// a A0..A4 y emite el ecall de mmap
void emit_mmap() {
  create_symbol_table_entry(GLOBAL_TABLE, string_copy("mmap"),
    0, PROCEDURE, UINT64_T, 5, code_size);
  emit_load(REG_A0, REG_SP, 0); // addr
  emit_addi(REG_SP, REG_SP, WORDSIZE);
  emit_load(REG_A1, REG_SP, 0); // length
  ...
  emit_addi(REG_A7, REG_ZR, SYSCALL_MMAP);
  emit_ecall();
  emit_jalr(REG_ZR, REG_RA, 0);
}
```

---

## SLIDE 13 — `mmap`: el lado del emulador (`implement_mmap`), paso a paso

**[Los 8 pasos que arman el mapping]**

Texto para la slide:
- Esta es la función más larga de las tres syscalls. Conecta todo lo visto hasta ahora: valida la región (slide 11), pide frames al page cache (slide 10), y escribe la tabla de páginas.
- El paso clave es el 5: usa `set_PTE_for_page`, no `map_page` (se explica en la slide 14, por qué importa).
- Si la región es inválida, la syscall no crasea: devuelve `-1` codificado con `sign_shrink`, la misma convención que usan `open`/`read`/`write` para reportar error.

Diagrama (flujo de los 8 pasos):

```
 1. Leer addr,length,prot,fd,offset de A0..A4
             │
 2. length = round_up(length, PAGESIZE)
             │
 3. si addr == 0 → addr = mmap_next del proceso
             │
 4. is_valid_mmap_region(addr, length)? ──no──► A0 = -1, retorna (slide 11)
             │ sí
 5. por cada página:
       frame = get_or_load_cache_frame(fd, offset+i*PAGESIZE)   (slide 10)
       set_PTE_for_page(page_table, page, frame)   ◄── clave (ver slide 14)
             │
 6. avanzar mmap_next si addr+length > mmap_next actual
             │
 7. crear el nodo mapping y agregarlo al frente de context->mappings
             │
 8. A0 = addr (dirección donde quedó el mapping), retorna
```

Código:
```c
// selfie.c ~8407-8484
// #implementacion mmap: crea el mapping, valida la region y mapea cada
// pagina virtual a su cache frame
addr   = *(get_regs(context) + REG_A0);
length = round_up(*(get_regs(context) + REG_A1), PAGESIZE);
if (addr == 0) addr = get_mmap_next(context);
if (is_valid_mmap_region(context, addr, length) == 0) {
  *(get_regs(context) + REG_A0) = sign_shrink(-1, SYSCALL_BITWIDTH);
  set_pc(context, get_pc(context) + INSTRUCTIONSIZE);
  return;
}
while (i < number_of_pages) {
  page_offset_in_file = offset + i * PAGESIZE;
  frame = get_or_load_cache_frame(fd, page_offset_in_file);
  set_PTE_for_page(get_pt(context), page, frame);   // no map_page (slide 14)
  i = i + 1;
}
if (addr + length > get_mmap_next(context))
  set_mmap_next(context, addr + length);
mapping = allocate_mapping();
set_mapping_vaddr(mapping, addr); set_mapping_length(mapping, length);
set_mapping_prot(mapping, prot);  set_mapping_fd(mapping, fd);
set_mapping_offset(mapping, offset);
set_next_mapping(mapping, get_mappings(context));
set_mappings(context, mapping);
*(get_regs(context) + REG_A0) = addr;
```

Nota de producción: con `debug_mmap` activado, esta función imprime una línea por página mapeada y una línea de resumen final (`"mmap created mapping of N bytes at ... for fd ... at file offset ... with prot ..."`) — buen candidato para mostrar en la demo.

---

## SLIDE 14 — Núcleo B: `set_PTE_for_page` vs `map_page`

**[La decisión de diseño más importante del proyecto]**

Texto para la slide:
- Selfie ya tenía `map_page()`, usada para páginas normales de code/data/heap/stack. Pero `map_page()` **también actualiza** `lowest_lo_page`/`highest_lo_page`/`lowest_hi_page`/`highest_hi_page`.
- Esos 4 campos son justo los que usa `implement_fork` para decidir **qué rango de páginas copiar físicamente** al crear un hijo (lo vas a ver literal en la slide 18: fork recorre `[lowest_lo_page, highest_lo_page)` y `[lowest_hi_page, highest_hi_page)` para copiar).
- Si `implement_mmap` usara `map_page()`, cada `fork` duplicaría físicamente las páginas del mmap → se perdería el objetivo del proyecto (compartir el archivo entre procesos, no clonarlo).
- Por eso se usa la operación de más bajo nivel, `set_PTE_for_page()`, que solo escribe la entrada en la tabla de páginas del proceso actual, sin tocar esos 4 contadores. `fork` simplemente nunca "ve" esas páginas como algo que deba copiar — y por eso hay que copiarlas a mano en `implement_fork` (slide 19).

Diagrama (comparación):

```
              map_page(context, page, frame)         set_PTE_for_page(pt, page, frame)
              ────────────────────────────           ─────────────────────────────────
  escribe la entrada en                sí                          sí
  la tabla de páginas

  actualiza lowest/highest              sí (!)                      no
  _lo_page / _hi_page

  ¿lo ve implement_fork como           sí → LO COPIA               no → LO IGNORA, hay que
  "página a copiar"?                   físicamente al hijo         copiarlo a mano (slide 19)

  ¿el frame termina compartido         no, cada proceso           sí → LO COMPARTE
  entre padre e hijo?                  tiene su propia copia      (mismo frame físico)

  usado para                     code/data/heap/stack           páginas de mmap
                                  (regiones privadas)             (regiones compartibles)
```

Código:
```c
// selfie.c ~8455-8457 (implement_mmap, paso 5)
// set_PTE_for_page (no map_page): no toca lowest/highest_lo_page ni
// hi_page, asi que fork comparte el frame en vez de duplicarlo
set_PTE_for_page(get_pt(context), page, frame);
// NO: map_page(context, page, frame);
```

---

## SLIDE 15 — Traducción de direcciones: acceder a memoria mapeada

**[Cuando la CPU hace load/store sobre una dirección mmap]**

Texto para la slide:
- Una vez creado el mapping, el proceso simplemente hace `*addr` o `*addr = valor` como cualquier otra dirección. No hay una instrucción especial.
- La traducción sigue el camino normal: dirección virtual → página → tabla de páginas → frame físico. La única diferencia es que ese frame es, en realidad, una entrada del page cache.
- No se tocó el traductor de direcciones de Selfie (`load_virtual_memory`/`store_virtual_memory`): se reutiliza tal cual. Vale la pena decirlo explícitamente en la slide: solo se rellenó la tabla de páginas de otra forma (con `set_PTE_for_page` apuntando a un cache frame).

Diagrama (aplicando tu imagen 1 a un acceso concreto):

```
  addr = 0x40000000 + 10   (dentro del mapping, página 0, byte 10)
        │
        ▼
  página = get_page_of_virtual_address(addr)  →  página del mapping
        │
        ▼
  tabla de páginas del proceso
  ┌──────────────────┐
  │ página → frame F_0 │  ← escrito por set_PTE_for_page en el mmap (slide 13)
  └──────────────────┘
        │
        ▼
  dirección física = frame F_0 + offset dentro de la página (10)
        │
        ▼
  load/store normal sobre esa dirección física
  (el proceso está, sin saberlo, leyendo/escribiendo el cache frame
   que contiene el contenido del archivo)
```

Código: no hay una función nueva para esto — es la traducción normal de Selfie, reutilizada tal cual.

---

## SLIDE 16 — Validar permisos en cada acceso (`is_valid_segment_read`/`write`)

**[El mapping no es de fiar sin chequear su prot en cada acceso]**

Texto para la slide:
- Selfie ya clasificaba cada dirección como data/stack/heap antes de permitir el acceso, llevando un contador de lecturas/escrituras por segmento (`data_reads`, `stack_writes`, etc.). Se agregó una **cuarta categoría**: región mmap, con sus propios contadores `mmap_reads`/`mmap_writes`, siguiendo exactamente el mismo patrón que ya existía.
- Si la dirección no es data/stack/heap, se busca si pertenece a un mapping (`find_mapping_for_address`, slide 8) y se valida su `prot`.
- Un mapping de solo lectura (`prot=0`) rechaza escrituras con `segmentation fault`; uno de solo escritura (`prot=1`) rechaza lecturas; `prot=2` (lectura/escritura) permite ambas.

Diagrama (árbol de decisión, ejemplo para lectura):

```
       is_valid_segment_read(vaddr)
                  │
   ┌─────────┬────┴────┬──────────────┐
   │         │          │              │
  data     stack      heap      (ninguna anterior)
   │         │          │              │
   ▼         ▼          ▼              ▼
  OK        OK         OK     mapping = find_mapping_for_address(vaddr)
  data_reads++  stack_reads++  heap_reads++     │
                                    ┌────────────┴────────────┐
                                 mapping == 0            mapping != 0
                                    │                          │
                              segmentation fault      prot != MMAP_PROT_WRITE ?
                                                                │
                                                  ┌─────────────┴─────────────┐
                                                 sí (read u/o RW)          no (solo write)
                                                  │                            │
                                                 OK, mmap_reads++      segmentation fault
```

Código:
```c
// selfie.c ~2149-2151
// #implementacion mmap: contadores de lecturas/escrituras en la region
// mmap, mismo patron que data/stack/heap
uint64_t mmap_reads  = 0;
uint64_t mmap_writes = 0;

// selfie.c ~12234-12264
uint64_t is_valid_segment_read(uint64_t vaddr) {
  if (is_data_address(...)) { data_reads = data_reads + 1; return 1; }
  else if (is_stack_address(...)) { stack_reads = stack_reads + 1; return 1; }
  else if (is_heap_address(...)) { heap_reads = heap_reads + 1; return 1; }
  else {
    // #implementacion mmap: si la direccion no es data/stack/heap, revisa
    // si pertenece a una region mmap con permiso de lectura
    mapping = find_mapping_for_address(current_context, vaddr);
    if (mapping != 0)
      if (get_mapping_prot(mapping) != MMAP_PROT_WRITE) {
        mmap_reads = mmap_reads + 1;
        return 1;
      }
    return 0;
  }
}
```

Nota: `is_valid_segment_write` es simétrico, comparando contra `MMAP_PROT_READ` en vez de `MMAP_PROT_WRITE`. `test4` valida que un mapping `prot=0` rechace escritura con `segmentation fault`.

---

## SLIDE 17 — El mismo chequeo también en `copy_buffer`

**[Un mapping de solo lectura no se puede saltar por atrás con read()/write()]**

Texto para la slide:
- `is_valid_segment_read/write` (slide 16) protege los `load`/`store` de la CPU. Pero Selfie también tiene su propia syscall `read`/`write`, que usa `copy_buffer` para mover datos entre el host y la memoria virtual del proceso.
- Sin este chequeo, un proceso podría "colarse" y escribir un mapping de solo lectura llamando a `write()` en vez de hacer un `store` directo.
- Se agregó el mismo control de `prot` dentro de `copy_buffer`, cerrando ese hueco. También se extendió `copy_buffer` para que reconozca direcciones mmap como "segmento conocido" — antes solo reconocía data/stack/heap — usando `find_mapping_for_address` (slide 8) combinado con `is_data_stack_heap_address` mediante `if`/`else if` (de nuevo, C* no tiene `||`).

Diagrama:

```
                    intento de modificar un mapping prot=READ
                                    │
              ┌─────────────────────┴─────────────────────┐
              │                                             │
     store directo de la CPU                    write() / read() del kernel
     (ej: *addr = 5)                             (ej: write(fd_prog, addr, n))
              │                                             │
     is_valid_segment_write()                     copy_buffer() valida prot
     lo rechaza (slide 16)                         del mapping, lo rechaza también
              │                                             │
              └─────────────► segmentation fault ◄──────────┘
```

Código:
```c
// selfie.c ~7767-7791 (copy_buffer)
mapping = find_mapping_for_address(context, vaddr);

// #implementacion mmap: reconoce tambien direcciones dentro de un mapping
// mmap, ademas de data/stack/heap (C* no soporta ||, por eso if/else if)
if (is_data_stack_heap_address(context, vaddr))
  is_known_segment = 1;
else if (mapping != (uint64_t*) 0)
  is_known_segment = 1;
else
  is_known_segment = 0;

if (is_known_segment) {
  // #implementacion mmap: aplica el permiso (prot) del mapping tambien a
  // los read/write del kernel emulado, no solo a los store/load de la CPU
  if (mapping != (uint64_t*) 0) {
    if (upload) {
      if (get_mapping_prot(mapping) == MMAP_PROT_READ) {
        printf("%s: virtual address 0x%08lX is read-only\n", selfie_name, vaddr);
        return 0;
      }
    } else if (get_mapping_prot(mapping) == MMAP_PROT_WRITE) {
      printf("%s: virtual address 0x%08lX is write-only\n", selfie_name, vaddr);
      return 0;
    }
  }
  ...
}
```

---

## SLIDE 18 — `fork`: lo que Selfie ya copiaba, antes de mmap

**[Punto de partida: cómo funcionaba fork sin mappings]**

Texto para la slide:
- Antes de este proyecto, `implement_fork` ya copiaba al hijo el `pc`, los registros, y el contenido de memoria del padre — pero no todo, solo un rango: las páginas entre `lowest_lo_page`/`highest_lo_page` (segmento "lo", típicamente code+data) y `lowest_hi_page`/`highest_hi_page` (segmento "hi", típicamente heap+stack).
- Esos 4 límites son exactamente los que actualiza `map_page()` cada vez que mapea una página nueva (slide 14) — por eso el mmap, al usar `set_PTE_for_page` en vez de `map_page`, queda automáticamente **fuera** de este mecanismo de copia.
- Conclusión: sin cambios adicionales, un hijo de `fork` heredaría el código/datos/heap/stack del padre como siempre, pero **no vería nada de sus mappings de mmap** — habría que agregar esa herencia a mano (slide 19).

Diagrama:

```
  implement_fork (comportamiento ya existente, sin tocar)
  ┌──────────────────────────────────────────────────────┐
  │ page = lowest_lo_page(padre)                          │
  │ while page < highest_lo_page(padre):                  │
  │     map_and_store(hijo, vaddr, load_virtual_memory(...))│  ← copia real,
  │     page = page + 1                                   │     byte a byte
  │                                                        │
  │ page = lowest_hi_page(padre)                           │
  │ while page < highest_hi_page(padre):                  │
  │     map_and_store(hijo, vaddr, load_virtual_memory(...))│
  │     page = page + 1                                   │
  │                                                        │
  │ copia lowest/highest_lo/hi_page tal cual al hijo       │
  └──────────────────────────────────────────────────────┘
                        │
                        ▼
       las páginas de mmap NUNCA tocaron estos 4 límites
       (set_PTE_for_page no los actualiza) → fork, tal cual
       existía, las ignora por completo. Falta agregarlas.
```

Código:
```c
// selfie.c ~8228-8248 (implement_fork, código ya existente en Selfie)
page = get_lowest_lo_page(context);
vaddr = page * PAGESIZE;
while (get_page_of_virtual_address(vaddr) < get_highest_lo_page(context)) {
  map_and_store(child, vaddr, load_virtual_memory(get_pt(context), vaddr));
  vaddr = vaddr + WORDSIZE;
}
// ... mismo patrón para lowest_hi_page / highest_hi_page ...
set_lowest_lo_page(child, get_lowest_lo_page(context));
set_highest_lo_page(child, get_highest_lo_page(context));
set_lowest_hi_page(child, get_lowest_hi_page(context));
set_highest_hi_page(child, get_highest_hi_page(context));
```

---

## SLIDE 19 — `fork`: heredar mappings sin duplicar frames

**[Padre e hijo comparten el mismo archivo mapeado]**

Texto para la slide:
- Este es el escenario que pide el enunciado como uno de los tres tests mínimos: dos procesos (emparentados por `fork`) modifican el mismo mapping y ven los cambios entre sí.
- A lo ya existente (slide 18), se le agregaron 3 pasos: copiar `mmap_next`, copiar cada nodo de `mappings` (con los mismos vaddr/length/prot/fd/offset), y —el punto clave— por cada página del mapping, copiar la **misma dirección de frame** al hijo con `set_PTE_for_page`. No se llama `palloc()` de nuevo ni se duplica contenido: el hijo apunta al mismo cache frame físico que el padre.

Diagrama:

```
   PADRE (context)                            HIJO (child, tras fork)
   ┌──────────────────┐                      ┌──────────────────┐
   │ mappings ─────────┼── copiado (mismos    │ mappings ─────────┼──►
   │ vaddr/length/prot/│   vaddr/length/prot/ │ vaddr/length/fd/  │
   │ fd/offset          │   fd/offset)         │ offset (nodo nuevo)│
   │                    │                      │                    │
   │ mmap_next ─────────┼── copiado tal cual──►│ mmap_next          │
   └─────────┬──────────┘                      └─────────┬──────────┘
             │                                            │
             ▼ tabla de páginas del padre                 ▼ tabla de páginas del hijo
      página X → frame F_0                          página X → frame F_0
                    └──────────── MISMO FRAME FÍSICO ────────────┘
                    (set_PTE_for_page en ambos, no map_page,
                     no se llama palloc() de nuevo)
```

Código:
```c
// selfie.c ~8258-8281 (implement_fork, código agregado)
// #implementacion mmap: el hijo hereda mmap_next y cada mapping del
// padre, compartiendo el mismo cache frame via set_PTE_for_page en vez
// de duplicarlo
set_mmap_next(child, get_mmap_next(context));

parent_mapping = get_mappings(context);
while (parent_mapping != (uint64_t*) 0) {
  child_mapping = allocate_mapping();
  set_mapping_vaddr(child_mapping, get_mapping_vaddr(parent_mapping));
  set_mapping_length(child_mapping, get_mapping_length(parent_mapping));
  set_mapping_prot(child_mapping, get_mapping_prot(parent_mapping));
  set_mapping_fd(child_mapping, get_mapping_fd(parent_mapping));
  set_mapping_offset(child_mapping, get_mapping_offset(parent_mapping));
  set_next_mapping(child_mapping, get_mappings(child));
  set_mappings(child, child_mapping);

  // por cada página del mapping: mismo frame, no se duplica
  number_of_mapped_pages = get_mapping_length(parent_mapping) / PAGESIZE;
  j = 0;
  while (j < number_of_mapped_pages) {
    mapped_page = get_page_of_virtual_address(get_mapping_vaddr(parent_mapping)) + j;
    set_PTE_for_page(get_pt(child), mapped_page,
      get_frame_for_page(get_pt(context), mapped_page));
    j = j + 1;
  }
  parent_mapping = get_next_mapping(parent_mapping);
}
```

Test que lo valida: `test3_fork_comparte_frame.c` — el hijo escribe, el padre lee la misma dirección mapeada y detecta el cambio (exit code 88).

---

## SLIDE 20 — `munmap`: deshacer el mapping sin tocar el archivo

**[Solo se recibe addr, no length: ¿por qué?]**

Texto para la slide:
- A diferencia de Linux, esta versión simplificada de `munmap` no recibe `length`, porque ya quedó guardado en el mapping cuando se creó con `mmap`.
- `munmap` limpia la tabla de páginas de cada página del mapping (`set_PTE_for_page(..., 0)`, el mismo valor que Selfie interpreta como "no mapeada") y desconecta el nodo de `context->mappings`, buscando el nodo anterior para poder desengancharlo (lista simplemente enlazada).
- **No llama a `pfree()`**: el frame físico no se libera, porque otro proceso (por ejemplo un hijo del `fork` de la slide 19) puede seguir usándolo a través del page cache compartido.
- Detalle de C*: como el lenguaje no soporta `break`, el recorrido de la lista fuerza el fin del `while` poniendo el puntero de recorrido en `0` apenas encuentra el nodo buscado.

Diagrama:

```
                  munmap(addr)
                       │
   buscar en context->mappings el nodo con vaddr == addr
   (recorrido "sin break": al encontrarlo, se fuerza el fin
    del while y se guarda aparte en found_mapping)
                       │
        ┌──────────────┴──────────────┐
    no se encontró                se encontró
        │                              │
    retorna -1                por cada página del mapping:
    (error)                     set_PTE_for_page(pt, page, 0)
                                      │
                              desconectar el nodo de
                              context->mappings
                                      │
                              retorna 0 (éxito)

   IMPORTANTE: el frame físico (cache frame) NO se libera.
   Puede seguir compartido con otro proceso (ver page cache, slide 9).
```

Código:
```c
// selfie.c ~8541-8550 (implement_munmap)
while (mapping != (uint64_t*) 0) {
  if (get_mapping_vaddr(mapping) == addr) {
    found_mapping = mapping;
    // C* no soporta 'break': se fuerza el fin del while poniendo mapping a 0
    mapping = (uint64_t*) 0;
  } else {
    previous_mapping = mapping;
    mapping = get_next_mapping(mapping);
  }
}

// selfie.c ~8566
set_PTE_for_page(get_pt(context), page, 0);
// frame = 0 → is_virtual_address_mapped()/get_frame_for_page() lo leen
// como "página no mapeada". No se llama a pfree().
```

Tests que lo validan: `test6` (acceder tras `munmap` da segfault), `test7` (addr inválida retorna error), `test8` (el `munmap` del hijo no afecta el frame compartido con el padre).

---

## SLIDE 21 — `msync`: el único punto que escribe al disco

**[Hasta acá, todo vivía solo en memoria]**

Texto para la slide:
- Ni `mmap` ni las escrituras del proceso tocan el archivo. Solo `msync(addr)` lo hace.
- Recorre las páginas del mapping, obtiene el frame físico de cada una, y lo escribe al archivo con `lseek`+`write` **del host directamente** (los mismos builtins de la slide 3) — no usa la syscall `write` interna de Selfie.
- Por qué directo: `implement_msync` ya tiene la dirección **física** (el frame). La syscall `write` interna está pensada para traducir direcciones virtuales de un proceso invitado; usarla acá sería traducir de más, sobre algo que ya es físico.

Diagrama:

```
                  msync(addr)
                       │
   buscar en context->mappings el nodo con vaddr == addr
   (mismo patrón "sin break" que munmap)
                       │
        ┌──────────────┴──────────────┐
    no se encontró                se encontró
        │                              │
    retorna -1                fd = mapping.fd
    (error)                   base_offset = mapping.offset
                               n_pages = mapping.length / PAGESIZE
                                      │
                       por cada página i de 0..n_pages:
                         frame = get_frame_for_page(pt, page)
                         file_offset = base_offset + i*PAGESIZE
                         lseek(fd, file_offset, SEEK_SET)
                         write(fd, frame, PAGESIZE)   ◄── write() del HOST
                                      │
                               retorna 0 (éxito)
```

Código:
```c
// selfie.c ~8666-8692 (implement_msync)
fd          = get_mapping_fd(mapping);
base_offset = get_mapping_offset(mapping);
number_of_pages = get_mapping_length(mapping) / PAGESIZE;
while (i < number_of_pages) {
  page = get_page_of_virtual_address(addr) + i;
  frame = get_frame_for_page(get_pt(context), page);
  file_offset = base_offset + i * PAGESIZE;
  lseek(fd, file_offset, SEEK_SET);
  write(fd, (uint64_t*) frame, PAGESIZE);   // write() del host, no la syscall emulada
  i = i + 1;
}
```

Tests que lo validan: `test9` (escribe, hace `msync`, `od -c` confirma el cambio en disco), `test10` (control: sin `msync` el archivo queda intacto), `test11` (addr inválida retorna error sin crashear).

---

## SLIDE 22 — Cierre: resumen del flujo + demo

**[De la dirección virtual al disco: el recorrido completo]**

Texto para la slide:
- Recapitulación en una sola oración por syscall:
  - `mmap`: valida la región, pide frames al page cache (que puede reusar o cargar del archivo vía `lseek`+`read`), y escribe la tabla de páginas con `set_PTE_for_page`.
  - `munmap`: borra la entrada de la tabla de páginas y el nodo de `mappings`, pero deja el frame físico intacto por si otro proceso lo comparte.
  - `msync`: es la única función que efectivamente escribe al archivo, usando `lseek`+`write` del host.
  - `fork`: hereda mappings y comparte frames, nunca los duplica — precisamente porque `mmap` nunca usó `map_page()`.
- Los 11 tests entregados cubren los 3 escenarios mínimos del enunciado: lectura directa de memoria, escritura + persistencia con `msync`, y dos procesos (padre/hijo) compartiendo cambios sobre el mismo mapping.
- Cierre con demo en vivo: correr `test3_fork_comparte_frame.c` y `test9_msync_persiste_cambio_en_disco.c`.

Diagrama (el mapa de la slide 5, ahora completo, resumiendo el recorrido de las 3 syscalls sobre él):

```
                                 mmap                 msync
 dirección virtual ───────► tabla de páginas ───► cache frame ───────► archivo
 del proceso                (set_PTE_for_page)    (page cache,             ▲
                                    ▲              get_or_load_             │
                                    │              cache_frame)             │
                              munmap borra                                  │
                              esta flecha,                          único camino
                              NO toca el frame                      que persiste
                                                                     al disco
                                                                     (lseek+write)
```

Diapositiva final: tabla de los 11 tests (la misma que ya está en `fases.md`), con el exit code esperado de cada uno.

---

## Notas de producción para pasar esto a pptxgenjs

- Mantener el **tema oscuro** ya usado en HW5/HW6, con los diagramas ASCII de arriba convertidos a texto monoespaciado (fuente tipo `Consolas`/`Courier New`) dentro de shapes con fondo ligeramente distinto al de la slide, para que se lean como "diagramas de código".
- Los árboles de decisión (slides 7, 10, 11, 16) se pueden armar como shapes conectados en vez de ASCII puro si prefieres algo más visual — pero el ASCII de este plan ya te da la estructura exacta para no perder tiempo decidiendo el layout.
- Cada bloque de "Código" es candidato a un textbox con fondo tipo editor de código (línea de números a la izquierda si quieres el detalle) — los números de línea son aproximados a la versión de `selfie.c` que subiste (verificados con `grep -n "#implementacion"` sobre ese archivo); conviene re-verificar con `grep -n` antes de pegarlos en la slide por si el archivo se movió.
- Slide 2: el diagrama es el más denso de la presentación en términos de flechas — dale ancho completo y considera partirlo en dos mitades (SIN mmap / CON mmap) como dos shapes separados en vez de un solo bloque de texto, para que las flechas no se enreden visualmente.
- Slides 4, 5, 9, 15, 19, 21 son las que más se apoyan en diagrama (poco texto, diagrama grande) — dales más espacio visual que a las slides de código puro (12, 13, 20).
- Slides 18 y 19 están separadas a propósito: 18 es la "foto de antes" (fork sin mmap) para que el profesor entienda qué comportamiento ya existía, y 19 es "lo que se agregó" — evita mezclar código viejo y nuevo en la misma slide de fork.