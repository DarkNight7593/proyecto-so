// Test 2: escribe en un mapping y usa msync para persistir el cambio a disco.
// Valida que sin msync el archivo NO cambie, y con msync SI cambie.
// Exit esperado: 44
// Compilar: ./selfie -c test/test2_escritura_msync.c -m 4

uint64_t main() {
  uint64_t  ORIGINAL_VALUE;
  uint64_t  NEW_VALUE;
  uint64_t  fd_setup;
  uint64_t  fd_rw;
  uint64_t  fd_check;
  uint64_t* addr;
  uint64_t* buffer;
  uint64_t  before_ok;
  uint64_t  still_original_ok;
  uint64_t  persisted_ok;

  // "MmapInit" y "MsyncOK!" codificados como uint64_t 
  ORIGINAL_VALUE = 8388357042651360589;
  NEW_VALUE      = 2399098514978730829;

  // crear archivo con un valor conocido
  fd_setup = open("test/test2_escritura_msync.txt", 577, 420);
  buffer = malloc(8);
  *buffer = ORIGINAL_VALUE;
  write(fd_setup, buffer, 8);

  // abrir en lectura/escritura y mapear
  fd_rw = open("test/test2_escritura_msync.txt", 2, 0);
  addr = mmap(0, 4096, 2, fd_rw, 0);

  // lo leido por CPU coincide con el archivo
  before_ok = 0;
  if (*addr == ORIGINAL_VALUE)
    write(1, addr, 8);
    write(1, "\n", 1);
    before_ok = 1;

  // escribir solo en memoria (cache frame), archivo aun no cambia
  *addr = NEW_VALUE;

  // fd nuevo + lectura directa: confirma que el disco sigue igual
  fd_check = open("test/test2_escritura_msync.txt", 0, 0);
  buffer = malloc(8);
  read(fd_check, buffer, 8);
  still_original_ok = 0;
  if (*buffer == ORIGINAL_VALUE)
    still_original_ok = 1;

  // msync: unico punto que escribe el cache frame al archivo
  msync(addr);

  // fd nuevo + lectura directa: ahora si debe verse el cambio
  fd_check = open("test/test2_escritura_msync.txt", 0, 0);
  buffer = malloc(8);
  read(fd_check, buffer, 8);
  persisted_ok = 0;
  if (*buffer == NEW_VALUE)
    persisted_ok = 1;

  write(1, buffer, 8);

  if (before_ok)
    if (still_original_ok)
      if (persisted_ok)
        exit(44);

  exit(0);
}