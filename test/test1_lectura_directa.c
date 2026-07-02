// Test 1: crear un mapping de un archivo y leer su contenido directamente
// de memoria (acceso por CPU, do_load, sin pasar por read()).
// Compilar: ./selfie -c test/test1_lectura_directa.c -m 4
// Exit code esperado: 42
uint64_t main() {
  uint64_t  fd;
  uint64_t* addr;
  uint64_t  value;

  fd = open("test/testfile.txt", 0, 0);

  // prot=0: MMAP_PROT_READ (solo lectura)
  // la pagina viene zeroeada (palloc->zmalloc), no hace falta calcular length
  addr = mmap(0, 4096, 0, fd, 0);

  // acceso directo por CPU: dispara do_load -> is_valid_segment_read (M6)
  // lee los primeros 8 bytes del archivo como palabra de 64 bits
  value = *addr;

  // volcar los primeros 16 bytes a stdout (2 palabras)
  write(1, addr, 16);

  exit(42);
}