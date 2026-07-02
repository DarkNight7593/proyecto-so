// Test 7: fork() ANTES de mmap(); cada rama abre su propio fd del archivo.
// Valida que fd distintos generen cache frames distintos (no comparten).
// Exit esperado: 33
// Compilar: ./selfie -c test/test7_fd_distinto_no_comparte.c -m 4

uint64_t main() {
  uint64_t  ORIGINAL_VALUE;
  uint64_t  CHILD_VALUE;
  uint64_t  fd_setup;
  uint64_t  fd_own;
  uint64_t* addr_own;
  uint64_t* buffer;
  uint64_t  pid;
  uint64_t* status;

  ORIGINAL_VALUE = 111111111;
  CHILD_VALUE    = 222222222;

  // crear archivo con un valor conocido
  fd_setup = open("test/test7_fd_distinto_no_comparte.txt", 577, 420);
  buffer = malloc(8);
  *buffer = ORIGINAL_VALUE;
  write(fd_setup, buffer, 8);

  // fork ANTES de mmap: nadie hereda ningun mapping
  pid = fork();

  if (pid == 0) {
    // hijo: abre y mapea por su cuenta (fd propio)
    fd_own = open("test/test7_fd_distinto_no_comparte.txt", 2, 0);
    addr_own = mmap(0, 4096, 2, fd_own, 0);
    *addr_own = CHILD_VALUE;
    exit(1);
  } else {
    // padre: abre y mapea por su cuenta tambien (otro fd)
    fd_own = open("test/test7_fd_distinto_no_comparte.txt", 2, 0);
    addr_own = mmap(0, 4096, 2, fd_own, 0);

    status = malloc(8);
    wait(status);
    write(1, addr_own, 8);

    if (*addr_own == ORIGINAL_VALUE)
      exit(33);
    else
      exit(0);
  }
}
