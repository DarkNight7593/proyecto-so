# Cómo funciona `mmap` — explicación de la implementación

> Este documento explica, desde cero, cómo funciona la implementación de
> `mmap` en Selfie: qué es una página, qué es un frame físico, cómo se
> comparte memoria entre procesos, y cómo se conecta todo eso con el CPU
> emulado. No asume que el lector conoce el código.

---

## 0. La idea en una sola frase

`mmap` hace que el **contenido de un archivo se pueda leer y escribir como
si fuera memoria normal** (con `*puntero`), en vez de tener que llamar
`read()`/`write()` cada vez. Y como bonus, si dos procesos mapean el mismo
archivo, **comparten la misma copia en memoria física**, en lugar de tener
cada uno la suya.

---

## 1. Lo mínimo que hay que saber sobre memoria virtual

Cuando un programa hace `*p = 5`, el procesador real (o, en este caso, el
emulador de Selfie) **no escribe directamente en una celda de RAM con esa
dirección**. Hay una capa intermedia:

```
direccion que ve el programa          direccion real en RAM
   (direccion virtual)        ──────►    (direccion fisica)
        0x40000000                          quien sabe donde
```

Esa traducción se hace **de a bloques**, no byte por byte. Cada bloque se
llama **página**, y en Selfie cada página mide `PAGESIZE = 4096` bytes.

- El espacio de direcciones virtuales de un proceso está dividido en
  páginas virtuales (página 0 = bytes 0–4095, página 1 = bytes 4096–8191,
  etc.).
- La memoria física real también está dividida en bloques del mismo
  tamaño, llamados **frames** (marcos de página).
- Cada proceso tiene su propia **tabla de páginas**: una lista que dice
  "mi página virtual N vive en el frame físico F".

```
   Proceso A                          RAM fisica
  ┌───────────────┐    tabla de      ┌───────────────┐
  │ pagina virt.0 │───►pagina A─────►│ frame 17      │
  │ pagina virt.1 │───►pagina B─────►│ frame 3       │
  │ pagina virt.2 │───► (sin mapear) │ frame 9       │
  └───────────────┘                  │ frame 17  ◄───┘ (contenido real)
                                      └───────────────┘
```

Si una página virtual **no** tiene una entrada en la tabla (o su entrada
dice "frame 0" / "no mapeada"), cualquier acceso a esa dirección es un
error: **segmentation fault**.

Esto ya existe en Selfie para el código del programa, el stack y el heap.
Lo que se agregó es una **cuarta zona**: la región de `mmap`.

---

## 2. ¿Qué problema resuelve `mmap`?

**Sin `mmap`**, para leer un archivo hay que hacer esto cada vez:

```
abrir archivo
pedir un buffer
llamar read(fd, buffer, cantidad_de_bytes)   ← copia bytes del disco al buffer
usar buffer
```

Cada `read()` es una copia explícita. Si dos procesos quieren leer el mismo
archivo, cada uno hace su propia copia, aunque el contenido sea idéntico.

**Con `mmap`**, el archivo se "engancha" directamente al espacio de
direcciones del proceso:

```
addr = mmap(...)     ← una sola vez
valor = *addr         ← leer el archivo es leer memoria, sin llamar a read()
*addr = nuevo_valor   ← escribir el archivo es escribir memoria
```

Y si dos procesos mapean el mismo archivo en el mismo punto, **ambos
terminan apuntando al mismo bloque de memoria física** (mismo frame). No
hay dos copias — hay una sola, compartida.

---

## 3. Las tres piezas necesarias para lograr esto

Para que la idea de la sección 2 funcione hacen falta tres cosas:

| Pieza | Pregunta que responde |
|-------|------------------------|
| **A. Lista de mappings** (por proceso) | "¿Qué archivos mapeé yo, y en qué direcciones?" |
| **B. Page cache** (global, compartido) | "¿Ya hay un frame cargado con esta página de este archivo? ¿O hay que leerla del disco?" |
| **C. Cambio en el validador de memoria del CPU** | "¿Esta dirección que el programa quiere leer/escribir es válida?" |

Vamos una por una.

---

## 4. Pieza A — La lista de mappings (el "cuaderno personal" de cada proceso)

Cada proceso tiene su propia lista de "cosas que mapeé". Es una lista
enlazada simple, donde cada nodo (cada "mapping") guarda 6 datos:

```
┌─────────────────────────────────────────────┐
│ siguiente │ direccion │ tamaño │ permisos │  │
│ mapping   │  virtual  │        │  (prot)  │  │
│           │  inicial  │        │          │  │
├───────────┼───────────┼────────┼──────────┤  │
│           │           │        │ archivo  │ offset dentro │
│           │           │        │  (fd)    │  del archivo  │
└─────────────────────────────────────────────┘
```

Cuando el proceso llama `mmap(...)`, se crea uno de estos nodos y se agrega
a su lista. Cuando más adelante el programa hace `*p` sobre alguna
dirección, el sistema puede recorrer esta lista y preguntar: *"¿esta
dirección cae dentro del rango de alguno de mis mappings?"* Si sí, sabe
exactamente con qué archivo, qué offset y qué permisos está tratando.

Los permisos (`prot`) son simples:

| valor | significado |
|-------|-------------|
| `0` | solo lectura |
| `1` | solo escritura |
| `2` | lectura y escritura |

---

## 5. Pieza B — El page cache (la "biblioteca compartida")

Acá está la parte más importante para entender la **compartición**.

Pensemos en el page cache como una biblioteca: cuando alguien quiere leer
un libro (una página de un archivo), primero se pregunta *"¿ya hay una
copia de este libro en la sala de lectura?"*

- **Si ya hay una copia (cache hit):** se la presta, sin ir al depósito
  (el disco).
- **Si no hay copia (cache miss):** se va al depósito, se trae el libro
  (se lee del disco), se pone una copia en la sala de lectura para la
  próxima vez, y *esa misma copia* es la que se presta.

```
                    PAGE CACHE (global, compartido por TODOS los procesos)
                  ┌──────────────────────────────────────────┐
                  │ archivo=23, offset=0    → frame 17        │
                  │ archivo=23, offset=4096 → frame 4          │
                  └──────────────────────────────────────────┘
                            ▲                      ▲
                            │                       │
            Proceso A: mapea archivo 23 offset 0    │
            Proceso B: mapea archivo 23 offset 0 ───┘
            (ambos llegan al MISMO frame 17)
```

Cada entrada del page cache guarda 4 datos: el archivo, el offset de esa
página dentro del archivo, y la dirección del frame físico donde está
cargado el contenido.

La función que hace esto (llamémosla "buscar o cargar") sigue esta lógica:

```
buscar_o_cargar(archivo, offset):
    recorrer el page cache
    si encuentro una entrada con este archivo y este offset:
        devolver el frame que ya existe        ← cache hit, no toca el disco
    si no la encuentro:
        reservar un frame fisico nuevo (vacio)
        posicionar el cursor del archivo en "offset"
        leer 4096 bytes del archivo hacia ese frame
        guardar una entrada nueva en el page cache
        devolver ese frame nuevo                ← cache miss
```

Esto es lo que garantiza que **dos mappings del mismo archivo en el mismo
offset terminen en el mismo frame**: la segunda vez que alguien pide ese
`(archivo, offset)`, el page cache ya lo tiene y lo reutiliza, en vez de
crear una copia nueva.

---

## 6. Pieza C — El "policía de memoria" del CPU

Cada vez que el programa ejecuta una instrucción que lee o escribe memoria
(`*p` o `*p = x`), el emulador hace una pregunta antes de tocar nada:
*"¿esta dirección es válida para este proceso?"*

Antes de `mmap`, las únicas zonas válidas eran:

1. El segmento de **datos** (variables globales del programa)
2. El **stack** (variables locales)
3. El **heap** (memoria pedida con `malloc`)

Si una dirección no caía en ninguna de esas tres, el emulador la rechazaba
con un *segmentation fault* — **aunque la tabla de páginas tuviera una
entrada válida ahí**, porque la pregunta se hace *antes* de mirar la tabla
de páginas.

Por eso `mmap` necesita agregar una **cuarta zona válida**: si la
dirección cae dentro del rango de alguno de los mappings del proceso
(usando la lista de la Pieza A), entonces es válida — pero respetando el
permiso (`prot`) de ese mapping en particular:

```
¿la direccion es valida para LEER?
  ├─ esta en datos/stack/heap?           → si, valida
  └─ esta dentro de un mapping mmap?
        └─ ese mapping permite lectura (prot 0 o 2)?  → si, valida
        └─ ese mapping es solo-escritura (prot 1)?     → no, segfault

¿la direccion es valida para ESCRIBIR?
  ├─ esta en datos/stack/heap?           → si, valida
  └─ esta dentro de un mapping mmap?
        └─ ese mapping permite escritura (prot 1 o 2)? → si, valida
        └─ ese mapping es solo-lectura (prot 0)?        → no, segfault
```

Con este cambio, una vez que la dirección pasa esta validación, el resto
del camino es **exactamente el mismo que para cualquier otra memoria**: se
consulta la tabla de páginas, se encuentra el frame, se lee/escribe ahí.
`mmap` no necesitó inventar un mecanismo de carga/escritura nuevo — solo
necesitó que esta validación dejara pasar su zona.

---

## 7. El recorrido completo de una llamada a `mmap`, paso a paso

Supongamos: `addr = mmap(0, 4096, 2, fd, 0)` — "mapeame 4096 bytes del
archivo `fd`, desde el offset 0, con lectura y escritura, y elegí vos la
dirección".

1. **Redondear el tamaño** al múltiplo de 4096 más cercano (en este
   ejemplo ya es exactamente 4096, una página).
2. **Como pedí `addr = 0`**, el sistema usa la próxima dirección libre que
   tiene guardada para mí (la primera vez, eso es la dirección base de la
   zona de mmap).
3. **Calcular cuántas páginas** cubre el mapping: `4096 / 4096 = 1` página.
4. **Para esa página:** preguntarle al page cache (Pieza B) por
   `(fd, offset=0)`. El page cache devuelve un frame (nuevo o reusado).
5. **Anotar en mi tabla de páginas** que mi página virtual (la que
   corresponde a la dirección elegida) apunta a ese frame. Importante:
   esto se anota con una operación que **solo toca la tabla de páginas**,
   sin marcar esa zona como "memoria normal del proceso" — la razón de
   esto se explica en la sección 9 (fork).
6. **Avanzar mi "próxima dirección libre"** en 4096 bytes, para que si
   vuelvo a llamar `mmap` con `addr=0`, no me pise este mapping.
7. **Guardar un nodo en mi lista de mappings** (Pieza A) con todos los
   datos: dirección, tamaño, permisos, archivo, offset.
8. **Devolver la dirección** elegida como resultado de `mmap`.

En ningún momento de este proceso se escribió nada en el archivo — solo se
**leyó** (en el paso 4, y solo si era necesario). Esto es intencional:
`mmap` prepara la memoria, no sincroniza cambios hacia el disco.

---

## 8. Qué pasa cuando el programa hace `*addr` o `*addr = valor`

Una vez que `mmap` ya devolvió la dirección, el programa la usa como
cualquier puntero:

```
valor = *addr        // leer
*addr = nuevo_valor   // escribir
```

Esto **no pasa por `mmap` de nuevo**. Es una instrucción de carga/escritura
normal del procesador emulado, que sigue este camino:

```
*addr
  │
  ▼
¿la direccion es valida? (Pieza C)
  │
  ▼ si
buscar en mi tabla de paginas: ¿que frame le corresponde a esta direccion?
  │
  ▼
leer/escribir directamente en ese frame de memoria fisica
```

Como el frame es el mismo que está en el page cache, **cualquier cambio
que escriba ahí queda en ese frame** — y si otro proceso comparte ese
mismo frame (porque mapeó el mismo archivo/offset, o porque lo heredó por
`fork`), ese otro proceso va a ver el cambio inmediatamente, porque está
mirando exactamente la misma memoria física.

---

## 9. ¿Qué pasa con `fork`? (por qué hace falta cuidado extra)

`fork` crea un proceso hijo que es una copia del padre. Para la memoria
"normal" (datos, stack, heap), Selfie copia físicamente cada página: el
hijo recibe **su propia copia independiente**.

Pero para `mmap`, **no queremos eso**. Queremos que el hijo siga
compartiendo el mismo frame que el padre (si el padre escribe, el hijo
debe verlo, y viceversa) — exactamente como si fueran dos procesos que
mapearon el mismo archivo de forma independiente.

¿Cómo se logra? Acá entra el detalle de la sección 7, paso 5: cuando
`mmap` anota la página en la tabla, usa una operación que **no marca esa
zona como parte de la memoria que `fork` debe copiar físicamente**. Por
eso, cuando un proceso con mappings hace `fork`:

- Las páginas de datos/stack/heap se copian físicamente, como siempre.
- Las páginas de `mmap` **no entran en esa copia automática**, porque
  nunca quedaron marcadas como "memoria a copiar".
- Entonces `fork` hace, *además*, este paso extra solo para mmap:
  1. Copia la lista de mappings del padre al hijo (mismos datos: dirección,
     tamaño, permisos, archivo, offset — son entradas nuevas, pero con la
     misma información).
  2. Para cada página de cada mapping, mira en la tabla del padre qué
     frame le corresponde, y escribe **ese mismo número de frame** en la
     tabla del hijo.

Resultado: padre e hijo tienen **dos tablas de páginas distintas**, pero
ambas entradas de mmap apuntan **al mismo frame físico**. Es la misma idea
que el page cache (sección 5), aplicada al caso de procesos emparentados.

```
   Padre                              Frame fisico
  pagina virt. X ──► frame 17 ◄────┐
                                    │
   Hijo (despues de fork)          │
  pagina virt. X ──► frame 17 ─────┘   (mismo frame, dos tablas distintas)
```

---

## 10. Qué **no** hace esta implementación de `mmap`

Para que quede claro el alcance:

- **No escribe nada en el archivo en disco.** Todo lo que el programa
  escribe con `*addr = valor` queda en el frame de memoria física, no en
  el archivo. Persistir esos cambios al archivo es trabajo de otra syscall
  (no incluida en esta entrega), que recorrería los mappings y volcaría el
  contenido de cada frame de vuelta al archivo con `write`/`lseek`.
- **No libera memoria.** Los frames reservados con `mmap` quedan
  reservados mientras el programa corre (no hay una syscall en esta
  entrega que los libere ni que quite el mapping de la lista).
- **El identificador de archivo usado en el page cache es el descriptor de
  archivo (`fd`) tal como lo entrega el sistema operativo real.** Esto
  funciona perfecto para el caso de procesos emparentados por `fork` (que
  heredan el mismo `fd`), pero si dos procesos **no emparentados** abren
  el mismo archivo de forma independiente, cada uno recibe su propio `fd`
  distinto, y el page cache no los reconocería como "el mismo archivo" —
  terminarían con frames separados en vez de compartir uno solo.

---

## 11. Resumen visual de todo el flujo

```
            mmap(addr, length, prot, fd, offset)
                          │
                          ▼
        ┌─────────────────────────────────────┐
        │ 1. redondear length a multiplo de    │
        │    pagina; resolver addr=0           │
        ├─────────────────────────────────────┤
        │ 2. por cada pagina del rango:         │
        │    preguntarle al PAGE CACHE          │
        │    (hit: reusa frame / miss: lo carga)│
        ├─────────────────────────────────────┤
        │ 3. anotar pagina virtual → frame      │
        │    en MI tabla de paginas             │
        ├─────────────────────────────────────┤
        │ 4. guardar el mapping en MI LISTA     │
        │    (direccion, tamaño, prot, fd, off) │
        └─────────────────────────────────────┘
                          │
                          ▼
                  devuelve "addr"

   ...mas tarde, el programa hace *addr o *addr = valor...
                          │
                          ▼
        ┌─────────────────────────────────────┐
        │ ¿la direccion cae en algun mapping    │
        │  de MI LISTA, y el prot lo permite?   │
        └─────────────────────────────────────┘
                          │ si
                          ▼
              leer/escribir en el frame fisico
              correspondiente (el mismo que
              quedo registrado en el PAGE CACHE,
              y que puede estar compartido con
              otros procesos via fork o via otro
              mmap del mismo archivo/offset)
```