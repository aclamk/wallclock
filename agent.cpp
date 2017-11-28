/*
 * agent.cpp
 *
 *  Created on: Nov 16, 2017
 *      Author: adam
 */
#include <execinfo.h>
#include <sys/types.h>
#include <atomic>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#define local_assert(x) do { if(!x) *(char*)nullptr=0; } while(false)

extern "C" void grab_callstack(void);

/*
extern "C" void _start(void);
void _start(void)
{

}
*/
constexpr size_t callstack_items = 100;

std::atomic<bool> my_lock{false};

void grab_callstack(void)
{
  if(my_lock.exchange(true) == false)
  {
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_word_t ip, sp;

   // uint64_t start = now();

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    while (unw_step(&cursor) > 0) {
      unw_get_reg(&cursor, UNW_REG_IP, &ip);
      unw_get_reg(&cursor, UNW_REG_SP, &sp);
      char buffer[100];
      unw_word_t diff;
      unw_cursor_t cursor2 = {0};
      unw_set_reg(&cursor2, UNW_REG_IP, ip);
      //unw_get_proc_name(&cursor, buffer, 100, &diff);
      //printf ("ip = %lx, sp = %lx %s[%ld]\n", (long) ip, (long) sp, buffer, diff);
    }
    //uint64_t end = now();
    //printf("show_backtrace() took %luus\n",end-start);


    local_assert(my_lock.exchange(false) == true);
  }
  else
  {
    /*ignore if requested too soon*/
    /*TODO: notify somehow*/
  }
}

/*
int main(int argc, char** argv)
{
  grab_callstack(10);
  grab_callstack(100);
}
*/
