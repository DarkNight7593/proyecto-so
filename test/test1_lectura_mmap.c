// Test 1: crea un mapping de solo lectura y lee su contenido con *addr.
// Valida que mmap + la traduccion de direcciones (do_load) funcionen.
// Exit esperado: 42
// Compilar: ./selfie -c test/test1_lectura_mmap.c -m 4

uint64_t main() {
  uint64_t  ORIGINAL_VALUE;
  uint64_t  fd_setup;
  uint64_t  fd;
  uint64_t* addr;
  uint64_t* buffer;
  uint64_t  value;

  // "HolaOS!!" codificado como uint64_t
  ORIGINAL_VALUE = 2387280877885091656;

  // crear archivo con un valor conocido
  fd_setup = open("test/test1_lectura_mmap.txt", 577, 420);
  buffer = malloc(8);
  *buffer = ORIGINAL_VALUE;
  write(fd_setup, buffer, 8);

  // abrir de solo lectura y mapear (prot=0 -> solo lectura)
  fd = open("test/test1_lectura_mmap.txt", 0, 0);
  addr = mmap(0, 4096, 0, fd, 0);

  // lectura directa por CPU (sin pasar por read())
  value = *addr;
  write(1, addr, 8);

  if (value == ORIGINAL_VALUE)
    exit(42);
  else
    exit(0);
}