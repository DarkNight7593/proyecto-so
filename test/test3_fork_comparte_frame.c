// Test 3: dos procesos (padre e hijo, via fork) comparten el mapping del
// mismo archivo y pueden observar los cambios del otro en memoria, porque
// ambos apuntan al mismo cache frame fisico (Fase M7).
// Compilar y correr: ./selfie -c test3_fork_comparte_frame.c -m 4
// Exit code esperado: 88
uint64_t main() {
  uint64_t fd;
  uint64_t* addr;
  uint64_t pid;
  uint64_t value;
  uint64_t i;

  fd = open("test/testfile.txt", 0, 0);
  addr = (uint64_t*) mmap(0, 4096, 2, fd, 0);

  pid = fork();

  if (pid == 0) {
    // hijo: escribe un valor nuevo en la region heredada de mmap
    *addr = 555555;
    return 0;
  } else {
    // padre: espera (poll) a que el cambio del hijo sea visible en SU
    // propia tabla de paginas, lo que solo es posible si padre e hijo
    // comparten el mismo cache frame fisico
    i = 0;
    value = *addr;
    while (value != 555555) {
      if (i > 10000000)
        return 1;
      value = *addr;
      i = i + 1;
    }
    return 88;
  }
}
