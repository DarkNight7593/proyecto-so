// Test 8: munmap/msync con direccion invalida, y verifica que un munmap
// real SI le quita el acceso a la propia pagina del proceso (no solo
// borra el nodo de la lista de mappings).
// Valida manejo de errores + el efecto real de munmap sobre la tabla de paginas.
// Resultado esperado: se ven dos veces 4294967295 en pantalla (errores
// correctos = sign_shrink(-1,32)), luego segmentation fault.
// Compilar: ./selfie -c test/test8_errores_y_automunmap.c -m 4

uint64_t main() {
  uint64_t  ORIGINAL_VALUE;
  uint64_t  fd_setup;
  uint64_t  fd_rw;
  uint64_t* addr;
  uint64_t* buffer;
  uint64_t* result;
  uint64_t  value;

  // "Selfie42" codificado como uint64_t (8 bytes little-endian) para que,
  // si se inspecciona el archivo, se vea texto legible y no simbolos raros
  ORIGINAL_VALUE = 3617627904049702227;

  // crear archivo con un valor conocido
  fd_setup = open("test/test8_errores_y_automunmap.txt", 577, 420);
  buffer = malloc(8);
  *buffer = ORIGINAL_VALUE;
  write(fd_setup, buffer, 8);

  // mapear el archivo
  fd_rw = open("test/test8_errores_y_automunmap.txt", 2, 0);
  addr = mmap(0, 4096, 2, fd_rw, 0);

  result = malloc(16);

  // munmap sobre una direccion que no es el inicio del mapping -> error
  *result = munmap(addr + 1);

  // msync sobre una direccion invalida -> error tambien
  *(result + 1) = msync(addr + 1);

  // mostrar ambos codigos de error (deben ser 4294967295)
  write(1, result, 16);

  // ahora si, desmapear la direccion real del mapping
  munmap(addr);

  // acceder a la direccion ya desmapeada: debe fallar
  value = *addr;

  exit(1); // nunca se alcanza
}