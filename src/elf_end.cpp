#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <elfio/elfio_dump.hpp>

using namespace ELFIO;

int main( int argc, char** argv )
{
  if ( argc != 2 ) {
    std::cerr << "Usage: elf_end <elf file name>" << std::endl;
    return 1;
  }

  elfio reader;

  if ( !reader.load( argv[1] ) ) {
    std::cerr << "File " << argv[1] << " is not found or it is not an ELF file" << std::endl;
    return 1;
  }
  Elf64_Addr end = 0;
  for (size_t i = 0; i < reader.sections.size(); i++) {
    if (reader.sections[i]->get_flags() & SHF_ALLOC) {
      if (reader.sections[i]->get_address() + reader.sections[i]->get_size() > end)
        end = reader.sections[i]->get_address() + reader.sections[i]->get_size();
    }
  }
  std::cout << end << std::endl;
  return 0;
}
