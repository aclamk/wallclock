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

extern "C" void call_start(void (*entry_point)(), void (*exit_func)(), void* auxv, int auxv_size, int);
void atexit_x() {}

int load_agent(void*)
{
  int auxv_fd = open("/proc/self/auxv",O_RDONLY);
  char* auxv = (char*)malloc(1024);
  int auxv_size = read(auxv_fd, auxv, 1024);
  printf("auxv_fd=%d\n",auxv_fd);
  printf("auxv_size=%d\n",auxv_size);


  int fd = open("res/agent_wh_0x10000000",O_RDONLY);
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

  uint64_t entry_addr = *(uint64_t*)ptr;
  call_start((void(*)())entry_addr, (void(*)())atexit_x, auxv, auxv_size, 65536);
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
