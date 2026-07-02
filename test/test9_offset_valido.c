// Test 9: mismo archivo, mismo fd, tres mmap() con offset 0/4096/8192.
// Valida que mmap lea la pagina correcta del archivo segun offset
// (recrea la Tabla 1 del enunciado: mismo fd, distintos offsets -> distintos frames).
// Exit esperado: 93
// Compilar: ./selfie -c test/test9_offset_valido.c -m 4

uint64_t main() {
  uint64_t  PAGE0_VALUE;
  uint64_t  PAGE1_VALUE;
  uint64_t  PAGE2_VALUE;
  uint64_t  fd_setup;
  uint64_t  fd;
  uint64_t* page_buffer;
  uint64_t* addr0;
  uint64_t* addr1;
  uint64_t* addr2;
  uint64_t  ok0;
  uint64_t  ok1;
  uint64_t  ok2;

  PAGE0_VALUE = 100;
  PAGE1_VALUE = 200;
  PAGE2_VALUE = 300;

  // selfie zeroea la memoria de malloc en boot level >=1: el resto de
  // cada pagina (bytes 8..4095) queda en cero sin tocarlo a mano
  page_buffer = malloc(4096);

  fd_setup = open("test/test9_offset_valido.txt", 577, 420);

  // escribir 3 paginas seguidas (offsets 0, 4096, 8192), cada una con
  // su propio valor en la primera palabra
  *page_buffer = PAGE0_VALUE;
  write(fd_setup, page_buffer, 4096); // queda en el archivo en offset 0

  *page_buffer = PAGE1_VALUE;
  write(fd_setup, page_buffer, 4096); // queda en offset 4096

  *page_buffer = PAGE2_VALUE;
  write(fd_setup, page_buffer, 4096); // queda en offset 8192

  // mismo fd, tres mmap() con offset distinto cada uno
  fd = open("test/test9_offset_valido.txt", 0, 0);

  addr0 = mmap(0, 4096, 0, fd, 0);
  addr1 = mmap(0, 4096, 0, fd, 4096);
  addr2 = mmap(0, 4096, 0, fd, 8192);

  write(1, addr0, 8);
  write(1, addr1, 8);
  write(1, addr2, 8);

  // cada mapping debe leer la pagina que le corresponde a SU offset
  ok0 = 0;
  if (*addr0 == PAGE0_VALUE)
    ok0 = 1;

  ok1 = 0;
  if (*addr1 == PAGE1_VALUE)
    ok1 = 1;

  ok2 = 0;
  if (*addr2 == PAGE2_VALUE)
    ok2 = 1;

  if (ok0)
    if (ok1)
      if (ok2)
        exit(93);

  exit(0);
}