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
#include <iostream>
#include <syscall.h>

void largecode(void);
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg);

void* largecode(void*)
{
  std::cout << "TID = " << syscall(SYS_gettid) << std::endl;
  largecode();
  return nullptr;
}

bool load_agent()
{
  bool ret = false;
  int res;
  char* ptr;
  struct stat buf;
  int fd = open("pagent.rel",O_RDONLY);
  if (fd >= 0) {
    res = fstat(fd, &buf);
    if (res == 0) {
      ptr = (char*)mmap((void*)0x10000000, buf.st_size*2, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANON|MAP_FIXED, -1, 0);
      if (ptr) {
        res = read(fd, ptr, buf.st_size);
        if (res == buf.st_size) {
          //((void (*)())ptr)();
          ret = true;
        }
      }
    }
    close (fd);
  }
  return ret;
}

int main(int argc, char** argv)
{
  load_agent();
  for (int i=0;i<20;i++)
  {
    pthread_t thr;
    pthread_create(&thr, nullptr, largecode, nullptr);
  }
  largecode(nullptr);
  return 0;
}
