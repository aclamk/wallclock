#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <elfio/elfio_dump.hpp>

using namespace ELFIO;

int main( int argc, char** argv )
{
    if ( argc != 4 ) {
        printf( "Usage: copy_header <elf file name> <start address from map> <binary file name> \n" );
        return 1;
    }

    elfio reader;

    if ( !reader.load( argv[1] ) ) {
        printf( "File %s is not found or it is not an ELF file\n", argv[1] );
        return 1;
    }
    uint64_t entry_address = reader.get_entry();
    uint64_t segments_num = reader.segments.size();
    uint64_t segment_entry_size = reader.get_segment_entry_size();
    uint64_t segments_offset = reader.get_segments_offset();

    char bin_header[0x1000] = {0};

    int elf_fd = open(argv[1], O_RDONLY);
    if (elf_fd == -1){
      printf("Cannot open '%s'\n", argv[1]);
      return 1;
    }

    int r = pread(elf_fd, &bin_header[16], segments_num * segment_entry_size, segments_offset);
    if (r != segments_num * segment_entry_size) {
      printf("Problem reading '%s'\n", argv[1]);
      return 1;
    }

    entry_address = strtol(argv[2], nullptr, 0);
    printf("entry point: 0x%lx\n", entry_address);
    printf("segments offset: %ld\n", segments_offset);
    printf("segments count: %ld\n", segments_num);
    printf("segment size: %ld\n", segment_entry_size);

    *(uint64_t*)&bin_header[0] = entry_address;
    *(uint64_t*)&bin_header[8] = segments_num;


    int bin_fd = open(argv[3], O_WRONLY);
    if (bin_fd == -1){
      printf("Cannot open '%s'\n", argv[3]);
      return 1;
    }
    int w = pwrite(bin_fd, &bin_header[0], 0x1000, 0);
    if (w != 0x1000) {
      printf("Problem writing '%s'\n", argv[3]);
      return 1;
    }

    return 0;
}
