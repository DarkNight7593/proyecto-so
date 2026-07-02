// Test 5: el padre hace munmap pero el hijo sigue leyendo bien el frame.
// Valida que munmap NO libere el cache frame (sigue compartido con el hijo).
// Exit esperado: 66
// Compilar: ./selfie -c test/test5_munmap_no_libera_frame.c -m 4

uint64_t main() {
  uint64_t  ORIGINAL_VALUE;
  uint64_t  fd_setup;
  uint64_t  fd_rw;
  uint64_t* addr;
  uint64_t* buffer;
  uint64_t  pid;
  uint64_t* status;

  // "NoFree!!" codificado como uint64_t
  ORIGINAL_VALUE = 2387300763866394446;

  // crear archivo con un valor conocido
  fd_setup = open("test/test5_munmap_no_libera_frame.txt", 577, 420);
  buffer = malloc(8);
  *buffer = ORIGINAL_VALUE;
  write(fd_setup, buffer, 8);

  // mapear ANTES del fork, para que el hijo lo herede
  fd_rw = open("test/test5_munmap_no_libera_frame.txt", 2, 0);
  addr = mmap(0, 4096, 2, fd_rw, 0);

  pid = fork();

  if (pid == 0) {
    // hijo: solo lee, el frame debe seguir intacto
    write(1, addr, 8);
    if (*addr == ORIGINAL_VALUE)
      exit(77);
    else
      exit(0);
  } else {
    // padre: desmapea de inmediato, antes de esperar al hijo (a proposito)
    munmap(addr);

    status = malloc(8);
    wait(status);

    // exit code del hijo llega codificado como (codigo * 256)
    if (*status / 256 == 77)
      exit(66);
    else
      exit(0);
  }
}