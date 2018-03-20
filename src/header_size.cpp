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
        std::cerr << "Usage: size_header <elf file name>" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Prints size of ELF header + ELF sections" << std::endl;
        return 1;
    }

    elfio reader;
    if ( !reader.load( argv[1] ) ) {
        std::cerr << "File '" << argv[1] << "' is not found or it is not an ELF file" << std::endl;
        return 1;
    }

    uint64_t header_size =
        reader.get_segments_offset() +
        reader.segments.size() * reader.get_segment_entry_size();

    std::cout << header_size << std::endl;
    return 0;
}
