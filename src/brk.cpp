#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <errno.h>

extern "C"
int __brk(void *addr);

void* brk_base = nullptr;
size_t brk_size = 0;
void *__curbrk = 0;

int __brk(void *addr)
{
  if (brk_base == nullptr) {
    brk_size = 256*1024*1024;
    brk_base = (void*)syscall(SYS_mmap, 0, brk_size, PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_ANON|MAP_32BIT, -1, 0);
  }
  if (addr == nullptr) {
    __curbrk = brk_base;
    return 0;
  }
  if (addr >= brk_base && addr < brk_base + brk_size) {
    __curbrk = addr;
    return 0;
  }
  errno = ENOMEM;
  return -1;
}

int brk(void *addr) __attribute__ ((weak, alias ("__brk")));
