// Test 8: el hijo hace munmap de su copia del mapping; el padre, que
// comparte el mismo cache frame (heredado via fork), debe seguir pudiendo
// leer su propia copia del mapping sin problemas. Confirma que munmap no
// libera el frame fisico ni afecta la lista de mappings de otro proceso.
// Compilar y correr: ./selfie -c test8_munmap_no_afecta_a_otro_proceso.c -m 4
// Exit code esperado: 66
uint64_t main() {
  uint64_t fd;
  uint64_t* addr;
  uint64_t pid;
  uint64_t value;
  uint64_t i;
  uint64_t munmap_result;

  fd = open("test/testfile.txt", 0, 0);
  addr = (uint64_t*) mmap(0, 4096, 2, fd, 0);

  pid = fork();

  if (pid == 0) {
    // hijo: desmapea SU copia del mapping y termina
    munmap_result = munmap(addr);
    return 0;
  } else {
    // padre: espera un poco (busy loop) para dar tiempo a que el hijo
    // corra, y luego verifica que su propio mapping SIGUE funcionando
    i = 0;
    while (i < 5000000) {
      i = i + 1;
    }

    value = *addr;

    if (value == 7310221995137593160)
      return 66;
    else
      return 1;
  }
}
