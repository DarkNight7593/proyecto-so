// Test 6: crear un mapping, leerlo, hacer munmap, y verificar que un acceso
// posterior a esa direccion ya falla (la region dejo de existir).
// Compilar y correr: ./selfie -c test6_munmap_acceso_posterior_falla.c -m 4
// Resultado esperado: "segmentation fault at address 0x40000000"
uint64_t main() {
  uint64_t fd;
  uint64_t* addr;
  uint64_t  value;
  uint64_t  result;

  fd = open("test/testfile.txt", 0, 0);

  addr = (uint64_t*) mmap(0, 4096, 2, fd, 0);

  // leer antes de desmapear, debe funcionar
  value = *addr;

  result = munmap(addr);

  if (result != 0)
    return 1; // munmap deberia haber encontrado el mapping

  // este acceso debe fallar ahora, porque el mapping ya no existe
  value = *addr;

  return 2; // si llegamos aca, algo esta mal: deberia haber crasheado antes
}
