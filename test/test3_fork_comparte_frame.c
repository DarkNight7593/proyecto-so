// Test 3: fork() despues de mmap(); el hijo escribe y el padre ve el cambio.
// Valida que fork comparta el mismo cache frame (no lo duplique).
// Exit esperado: 88
// Compilar: ./selfie -c test/test3_fork_comparte_frame.c -m 4

uint64_t main() {
  uint64_t  ORIGINAL_VALUE;
  uint64_t  NEW_VALUE;
  uint64_t  fd_setup;
  uint64_t  fd_rw;
  uint64_t* addr;
  uint64_t* buffer;
  uint64_t  pid;
  uint64_t* status;

  // "ForkPre!" y "ChildWr!" codificados como uint64_t 
  ORIGINAL_VALUE = 2406455265625009990;
  NEW_VALUE      = 2410084839423830083;

  // crear archivo con un valor conocido
  fd_setup = open("test/test3_fork_comparte_frame.txt", 577, 420);
  buffer = malloc(8);
  *buffer = ORIGINAL_VALUE;
  write(fd_setup, buffer, 8);

  // mapear ANTES del fork, para que el hijo lo herede
  fd_rw = open("test/test3_fork_comparte_frame.txt", 2, 0);
  addr = mmap(0, 4096, 2, fd_rw, 0);
  write(1, addr, 8);
  write(1, "\n", 1);

  pid = fork();

  if (pid == 0) {
    // hijo: escribe sobre el frame compartido
    *addr = NEW_VALUE;
    exit(1);
  } else {
    // padre: espera al hijo y lee la MISMA direccion mapeada
    status = malloc(8);
    wait(status);
    write(1, addr, 8);
    write(1, "\n", 1);

    if (*addr == NEW_VALUE)
      exit(88);
    else
      exit(0);
  }
}