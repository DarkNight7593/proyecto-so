// Test 2: crear un mapping y escribir sobre el directamente en memoria
// (do_store), luego leer de vuelta (do_load) para confirmar que el cambio
// se ve reflejado en el cache frame.
// Compilar y correr: ./selfie -c test2_escritura_y_lectura.c -m 4
// Exit code esperado: 77
uint64_t main() {
  uint64_t fd;
  uint64_t* addr;
  uint64_t  value;

  fd = open("test/testfile.txt", 0, 0);

  addr = (uint64_t*) mmap(0, 4096, 2, fd, 0);

  *addr = 12345;

  value = *addr;

  if (value == 12345)
    return 77;
  else
    return 1;
}
