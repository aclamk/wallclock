/*
 * testprog.cpp
 *
 *  Created on: Nov 16, 2017
 *      Author: adam
 */

#include <string>
#include <stdlib.h>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

void largecode(void);
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg);

void* largecode(void*)
{
  std::cout << "TID = " << syscall(SYS_gettid) << std::endl;
  largecode();
  return nullptr;
}

int main(int argc, char** argv)
{
  for (int i=0;i<30;i++)
  {
    pthread_t thr;
    pthread_create(&thr, nullptr, largecode, nullptr);
  }
  largecode(nullptr);
  return 0;
}
