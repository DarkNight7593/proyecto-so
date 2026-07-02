// Test 4: dos mmap() del mismo proceso, mismo fd, comparten cache frame.
// Valida que el page cache indexe por (fd, offset), no por direccion virtual.
// Exit esperado: 55
// Compilar: ./selfie -c test/test4_dos_mmap_comparten_frame.c -m 4

uint64_t main() {
  uint64_t  ORIGINAL_VALUE;
  uint64_t  NEW_VALUE;
  uint64_t  fd_setup;
  uint64_t  fd;
  uint64_t* addr1;
  uint64_t* addr2;
  uint64_t* buffer;

  // "TwoMaps!" y "SameFrm!" codificados como uint64_t 
  ORIGINAL_VALUE = 2410393788786636628;
  NEW_VALUE      = 2408707022388027731;

  // crear archivo con un valor conocido
  fd_setup = open("test/test4_dos_mmap_comparten_frame.txt", 577, 420);
  buffer = malloc(8);
  *buffer = ORIGINAL_VALUE;
  write(fd_setup, buffer, 8);

  // un solo fd, dos mmap() distintos sobre el mismo offset
  fd = open("test/test4_dos_mmap_comparten_frame.txt", 2, 0);
  addr1 = mmap(0, 4096, 2, fd, 0);
  write(1, addr1, 8);
  write(1, "\n", 1);
  addr2 = mmap(0, 4096, 2, fd, 0);

  // escribir por addr1, leer por addr2, sin llamar msync
  *addr1 = NEW_VALUE;
  write(1, addr2, 8);

  if (*addr2 == NEW_VALUE)
    exit(55);
  else
    exit(0);
}