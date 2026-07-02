// Test 10 (control): escribir sobre la region mapeada SIN llamar msync NO
// debe persistir el cambio en el archivo en disco. Sirve para contrastar
// con el Test 9.
// Requiere: testfile_msync.txt (contenido inicial "AAAAAAAA")
// Compilar y correr: ./selfie -c test10_sin_msync_no_persiste.c -m 4
// Verificacion: "od -c testfile_msync.txt" debe seguir mostrando "AAAAAAAA"
uint64_t main() {
  uint64_t fd;
  uint64_t* addr;

  fd = open("test/testfile_msync.txt", 2, 0);

  addr = (uint64_t*) mmap(0, 4096, 2, fd, 0);

  *addr = 5136152271503443783; // "GGGGGGGG"

  // sin llamar msync: el cambio queda solo en el cache frame en memoria

  return 0;
}
