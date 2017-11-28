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

template <int depth, int x> struct do_log
{
  void log(void* cct);
};

template <int x> struct do_log<12, x>
{
  void log(void* cct);
};

template<int depth, int x> void do_log<depth,x>::log(void* cct)
{
  if (rand() % 2) {
    do_log<depth+1, x*2> log;
    log.log(cct);
  } else {
    do_log<depth+1, x*2+1> log;
    log.log(cct);
  }
}

std::string recursion(void* cct)
{
  return "here-recursion";
}

template<int x> void do_log<12, x>::log(void* cct)
{
  if ((rand() % 16000) == 0) {
    std::cout << "End " << recursion(cct) << "x=" << x << std::endl;
  } else {
    //std::cout << "End x=" << x << std::endl;
  }
}



#endif /* LARGECODE_H_ */
