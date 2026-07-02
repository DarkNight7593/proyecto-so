// Test 6: mapping de solo lectura (prot=0), se intenta escribir sobre el.
// Valida que is_valid_segment_write rechace la escritura.
// Resultado esperado: segmentation fault (no llega a exit)
// Compilar: ./selfie -c test/test6_prot_solo_lectura.c -m 4

uint64_t main() {
  uint64_t  ORIGINAL_VALUE;
  uint64_t  fd_setup;
  uint64_t  fd;
  uint64_t* addr;
  uint64_t* buffer;
  uint64_t  value;

  // "ReadOnly" codificado como uint64_t 
  ORIGINAL_VALUE = 8749489463339607378;

  // crear archivo con un valor conocido
  fd_setup = open("test/test6_prot_solo_lectura.txt", 577, 420);
  buffer = malloc(8);
  *buffer = ORIGINAL_VALUE;
  write(fd_setup, buffer, 8);

  // mapear de solo lectura
  fd = open("test/test6_prot_solo_lectura.txt", 0, 0);
  addr = mmap(0, 4096, 0, fd, 0);

  // la lectura si esta permitida
  value = *addr;
  write(1, addr, 8);

  // la escritura no: dispara segmentation fault (este valor nunca se
  // llega a imprimir, asi que se deja como numero)
  *addr = 999999999;

  exit(1); // nunca se alcanza
}