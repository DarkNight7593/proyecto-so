// Test 5: dos mappings distintos (dentro del mismo proceso) sobre el mismo
// archivo y el mismo offset deben compartir el mismo cache frame fisico,
// validando get_or_load_cache_frame (Fase M3, cache hit).
// Compilar y correr: ./selfie -c test5_dos_mmap_mismo_archivo_comparten_frame.c -m 4
// Exit code esperado: 99
uint64_t main() {
  uint64_t fd;
  uint64_t* addr1;
  uint64_t* addr2;
  uint64_t  value;

  fd = open("test/testfile.txt", 0, 0);

  addr1 = (uint64_t*) mmap(0, 4096, 2, fd, 0);
  addr2 = (uint64_t*) mmap(0, 4096, 2, fd, 0);

  *addr1 = 424242;

  value = *addr2;

  if (value == 424242)
    return 99;
  else
    return 1;
}
