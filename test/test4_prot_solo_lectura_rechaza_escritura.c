// Test 4: un mapping creado con prot=0 (solo lectura) debe rechazar
// escrituras (segmentation fault), validando que is_valid_segment_write
// respeta el permiso del mapping.
// Compilar y correr: ./selfie -c test4_prot_solo_lectura_rechaza_escritura.c -m 4
// Resultado esperado: "segmentation fault at address 0x40000000" (no exit 55)
uint64_t main() {
  uint64_t fd;
  uint64_t* addr;

  fd = open("test/testfile.txt", 0, 0);

  // prot = 0 (solo lectura)
  addr = (uint64_t*) mmap(0, 4096, 0, fd, 0);

  // esto debe ser rechazado por is_valid_segment_write
  *addr = 999;

  return 55;
}
