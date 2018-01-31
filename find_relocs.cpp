

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <vector>

int main( int argc, char** argv )
{
  if ( argc != 5 ) {
    std::cerr << "Usage: copy_header <binary@0> <binary@offset> <offset> <output relocations>" << std::endl;
    return 1;
  }

  int32_t relocation=0x12345000;
  char* endp;
  relocation = strtol(argv[3], &endp, 0);
  if (*endp != '\0') {
    std::cerr << "Invalid offset '" << argv[3] << "'" << std::endl;
    return 1;
  }
  int fd1 = open(argv[1],O_RDONLY);
  int fd2 = open(argv[2],O_RDONLY);

  int res;
  struct stat buf;
  res = fstat(fd1, &buf);
  int size1 = buf.st_size;
  res = fstat(fd2, &buf);
  int size2 = buf.st_size;

  if (size1 != size2) {
    std::cerr << "Sizes of '" << argv[1] << "' and '" << argv[2] << "' differ" << std::endl;
    return 1;
  }
  uint8_t* buf1 = new uint8_t[size1];
  uint8_t* buf2 = new uint8_t[size2];
  if (read(fd1, buf1, size1) != size1) {
    std::cerr << "Problem reading '" << argv[1] << "'" << std::endl;
    return 1;
  }
  if (read(fd2, buf2, size2) != size2) {
    std::cerr << "Problem reading '" << argv[2] << "'" << std::endl;
    return 1;
  }

  if (buf1[0] != buf2[0]) {
    std::cerr << "Input files garbled. Exiting." << std::endl;
    return 1;
  }
  std::vector <int32_t> relocs;
  for (int i=0; i<size1; i++)
  {
    uint32_t val1;
    uint32_t val2;

    if (buf1[i] == buf2[i]) continue;
    val1 = buf1[i-1] | (buf1[i]<<8) | (buf1[i+1]<<16) | (buf1[i+2]<<24);
    val2 = buf2[i-1] | (buf2[i]<<8) | (buf2[i+1]<<16) | (buf2[i+2]<<24);

    printf("%d %8.8x %8.8x %8.8x\n", i-1, val1, val2, val2-val1);
    if(val1-val2 != relocation && val2-val1 != relocation) {
      std::cerr << "@" << i << " mismatched offset" << std::endl;
      return 1;
    }
    //printf("%d %8.8x %8.8x %8.8x\n", i-1, val1, val2, val2-val1);
    if (val1<val2) relocs.push_back(i-1);
    if (val1>val2) relocs.push_back(-(i-1));
    i+=2;
  }

  int fdout;
  fdout = open(argv[4], O_WRONLY|O_CREAT, 0666);
  if (fdout == -1) {
    std::cerr << "Cant open '" << argv[4] << "' for writing" << std::endl;
    return 1;
  }
  int w;
  w = write(fdout, relocs.data(), relocs.size()*sizeof(int32_t));
  if (w != relocs.size()*sizeof(int32_t))
  {
    std::cerr << "Problem writing '" << argv[4] << "'" << std::endl;
    return 1;
  }
  close(fdout);
  return 0;
}
