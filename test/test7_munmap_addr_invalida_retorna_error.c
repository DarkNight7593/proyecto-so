// Test 7: munmap sobre una direccion que NO es el inicio de ningun mapping
// registrado debe devolver error (distinto de 0), sin crashear el emulador.
// Compilar y correr: ./selfie -c test7_munmap_addr_invalida_retorna_error.c -m 4
// Exit code esperado: 33
uint64_t main() {
  uint64_t fd;
  uint64_t* addr;
  uint64_t  result;

  fd = open("test/testfile.txt", 0, 0);

  addr = (uint64_t*) mmap(0, 4096, 2, fd, 0);

  // direccion que nunca fue mapeada
  result = munmap(addr + 999999);

  if (result != 0)
    return 33;
  else
    return 1;
}
