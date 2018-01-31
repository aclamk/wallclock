#include <inttypes.h>
#include <getopt.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

extern "C" void call_start(void*, void*, void*, int);
void atexit_x() {}

extern uint32_t _binary_rel_bin_start[0];
extern uint32_t _binary_rel_bin_end[0];

void apply_relocations(void* _binary_agent_bin_start)
{
  uint32_t diff = (uint32_t)(uint64_t)_binary_agent_bin_start;
  uint32_t* rel = _binary_rel_bin_start;
  while(rel != _binary_rel_bin_end)
  {
    int32_t offset = *rel;
    if(offset >= 0) {
      *(uint32_t*)(_binary_agent_bin_start + offset) += diff;
    } else {
      *(uint32_t*)(_binary_agent_bin_start - offset) -= diff;
    }
    rel++;
  }
}


int load_agent(void*)
{
  int auxv_fd = open("/proc/self/auxv",O_RDONLY);
  char* auxv = (char*)malloc(1024);
  int auxv_size = read(auxv_fd, auxv, 1024);
  printf("auxv_fd=%d\n",auxv_fd);
  printf("auxv_size=%d\n",auxv_size);


  int fd = open("agent_wh_0x00000000",O_RDONLY);
  printf("fd=%d\n",fd);
  int res;
  struct stat buf;
  res = fstat(fd, &buf);
  printf("fstat res=%d size=%d\n",res,buf.st_size);
  char* ptr;
  ptr = (char*)mmap((void*)0x10000000, buf.st_size*2, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANON|MAP_FIXED, -1, 0);

  printf("ptr=%p\n",ptr);
  res = read(fd, ptr, buf.st_size);
  printf("read %d\n",res);
  //((void (*)(...))(ptr+0x0000))(0, 0, nullptr, 0, 0, 0, 1, "abcdef", 0, "A=1", 0);


  uint64_t* p = (uint64_t*) auxv;
  size_t s = auxv_size / 16;
  for (int i = 0; i<s; i++)
  {
    if (p[i*2] == 3) p[i*2+1] = 0x10000010;
    if (p[i*2] == 5) p[i*2+1] = *(uint64_t*)(ptr+8);
  }

  apply_relocations(ptr);
  int fd1 = open("yyyyy_0x10000000", O_WRONLY|O_CREAT, 0666);
  write(fd1, ptr, buf.st_size);

  call_start((void*)*(uint64_t*)(ptr), (void*)atexit_x, auxv, auxv_size);
  printf("init call ended\n");
}
constexpr size_t stack_size = 1024 * 64;
char vstack[stack_size];

void _init_agent()
{
  pid_t v;
  if (clone(load_agent, vstack + stack_size,
                 CLONE_PARENT_SETTID | CLONE_FILES | CLONE_FS | CLONE_IO | CLONE_VM,
            nullptr, &v) == -1) {
  }
}


int main(int argc, char** argv)
{
  _init_agent();
  sleep(100000000);
}
