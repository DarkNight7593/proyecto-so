// Test 9: escribir sobre una region mapeada y luego llamar msync debe
// persistir el cambio en el archivo en disco (a diferencia de mmap solo,
// que mantiene los cambios unicamente en memoria).
// Requiere: testfile_msync.txt (8 bytes, contenido inicial "AAAAAAAA")
// Compilar y correr: ./selfie -c test9_msync_persiste_cambio_en_disco.c -m 4
// Exit code esperado: 44
// Verificacion adicional (fuera de selfie): "od -c testfile_msync.txt"
// debe mostrar "GGGGGGGG" (seguido de relleno en ceros hasta completar
// una pagina de 4096 bytes, porque msync escribe la pagina completa)
uint64_t main() {
  uint64_t fd;
  uint64_t* addr;
  uint64_t  result;

  // flags=2 (O_RDWR en Linux): el fd real del host necesita permiso de
  // escritura, ya que msync escribe sobre ese mismo fd
  fd = open("test/testfile_msync.txt", 2, 0);

  addr = (uint64_t*) mmap(0, 4096, 2, fd, 0);

  // escribe SOLO en memoria (el archivo todavia no cambia en este punto)
  *addr = 5136152271503443783; // "GGGGGGGG" en little-endian

  result = msync(addr);

  if (result == 0)
    return 44;
  else
    return 1;
}
