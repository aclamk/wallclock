#define _GNU_SOURCE
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>

/*
  @@init
  agent_init

  @@agent_header
    agent_entry_point (8b)
    agent_elf_header_num (8b)
    agent_elf_header

  @@agent_relocs
  @@stack
  ..align 4K..
  @@agent_binary

*/

extern uint64_t _binary_header_start[0];

extern uint32_t _binary_rel_bin_start[0];
extern uint32_t _binary_rel_bin_end[0];

extern char _stack_top[0];
extern char _stack_bottom[0];

extern char _binary_agent_nh_bin_start[0];
extern char _binary_agent_nh_bin_end[0];

long int raw_syscall (long int __sysno, ...);
void agent_thread_extract_param();

//char stack[65536];

void init_agent(uint64_t arg0)
{
  uint64_t* stack_bottom = (uint64_t*)_stack_bottom;
  //execution of child process begins by *return* from syscall after stack is set
  stack_bottom[-2] = (uint64_t)&agent_thread_extract_param;
  stack_bottom[-1] = arg0;
  //CLONE_PARENT makes that parent of profiled process gets sigchild. I hope it will not confuse any daemons.
  raw_syscall(SYS_clone, CLONE_PARENT_SETTID | CLONE_FILES | CLONE_FS | CLONE_IO | CLONE_VM | CLONE_PARENT,
                  stack_bottom - 2, 0, 0);
  //signal agent that init process is done
  raw_syscall(-1, arg0 );
}

//extern "C" void* agent_binary_begin;


void memcpy_r(char* dst, const char* src, size_t s)
{
  dst+=s;
  src+=s;
  while(s>0)
  {
    dst--; src--;
    *dst = *src;
    s--;
  }
}

int load_auxv(void* dst)
{
  int fd = raw_syscall(SYS_open, "/proc/self/auxv", O_RDONLY);
  if (fd == -1)
    return -1;
  int auxv_size = raw_syscall(SYS_read, fd, dst, 4096);
  raw_syscall(SYS_close, fd);
  return auxv_size;
}

void fix_auxv(uint64_t* p, size_t items)
{
  for (int i = 0; i<items; i++)
  {
    if (p[i*2] == 3) p[i*2+1] = (uint64_t)&_binary_header_start[2];
    if (p[i*2] == 5) p[i*2+1] = _binary_header_start[1];
  }
}

void apply_relocations(char* image_position, uint32_t diff)
{
  //uint32_t diff = (uint32_t)_binary_agent_nh_bin_start;
  uint32_t* rel = _binary_rel_bin_start;
  while(rel != _binary_rel_bin_end)
  {
    int32_t offset = *rel;
    if(offset >= 0) {
      *(uint32_t*)(image_position + offset) += diff;
    } else {
      *(uint32_t*)(image_position - offset) -= diff;
    }
    rel++;
  }
}
//void call_start();
void call_start(void (*entry_point)(), void (*exit_func)(), void* auxv, int auxv_size, int);
//{
//  ((void (*)())((uint32_t)_binary_agent_bin_start + _binary_header_start[0]))();
//}

void atexit_x() {

}

void agent_thread(uint64_t connection_id)
{
  struct timespec rqtp; //= { 40, 0 };
  rqtp.tv_sec = 30;
  //raw_syscall(SYS_nanosleep, &rqtp, 0);

  int auxv_size = load_auxv(_stack_top);
  if (auxv_size >= 0 && auxv_size < 4096) {
    fix_auxv((uint64_t*)_stack_top, auxv_size/(sizeof(uint64_t) * 2));
  }
  apply_relocations((char*)_binary_header_start, (uint32_t)(uint64_t)_binary_header_start);

  call_start((void (*)())_binary_header_start[0], (void (*)())atexit_x,
             _stack_top, auxv_size, connection_id);
  //call_agent(_stack_top, auxv_size);
}


#if 0
int main(int argc, char** argv)
{
  init_agent();
  _binary_agent_nh_bin_start[111]=111;
  return 0;
}
#endif
