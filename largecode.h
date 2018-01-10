/*
 * largecode.h
 *
 *  Created on: Nov 16, 2017
 *      Author: adam
 */

#ifndef LARGECODE_H_
#define LARGECODE_H_

#include <string>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <sys/syscall.h>
#include <assert.h>

template <int depth, int x> struct do_log
{
  int log(void* cct);
};

template <int x> struct do_log<6, x>
{
  int log(void* cct);
};

template<int depth, int x> int do_log<depth,x>::log(void* cct)
{
  int result;
  if (rand() % 2) {
    do_log<depth+1, x*2> log;
    result = log.log(cct);
  } else {
    do_log<depth+1, x*2+1> log;
    result = log.log(cct);
  }
  return result*x+depth;
}

std::string recursion(void* cct)
{
  char p[100];
  sprintf(p, "here-recursion tid=%ld",syscall(SYS_gettid));
  return p;
}

template<int x> int do_log<6, x>::log(void* cct)
{
  if ((rand() % 10) == 0) assert(write(0,0,0) == 0);
  if ((rand() % 100) == 0) usleep(3000);
  if ((rand() % 160000) == 0) {
    std::cout << "End " << recursion(cct) << " x=" << x << std::endl;
  } else {
    //std::cout << "End x=" << x << std::endl;
  }
  return 42;
}



#endif /* LARGECODE_H_ */
