#include <sys/ptrace.h>
#include <string>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>

#include <sys/user.h>

#include <execinfo.h>
#include <string>
#include <stdlib.h>
#include <iostream>
#include <syscall.h>
#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <atomic>
#include <semaphore.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>


#include "callstep.h"

extern "C"
int64_t _rax_test(uint64_t cnt);

uint64_t now()
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec*1000000000 + t.tv_nsec;
}

template <int depth, int x> struct do_log
{
  void log(void* cct);
};

template <int x> struct do_log<8, x>
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
void peek_results();
static std::atomic<uint32_t> counter{0};
//int32_t counter;
template<int x> void do_log<8, x>::log(void* cct)
{
  uint64_t i;
  //counter++;
  //i=counter;
  i=counter.fetch_add(1);
  if ((i % 10) == 0) write(0,0,0);
  if ((i % 1600000) == 0) {

    std::cerr << "-------" << now() << "--------------End " << recursion(cct) << "x=" << x << " stack=" << std::hex << &i << std::dec << std::endl;
    peek_results();

  } else {
          //std::cout << "End x=" << x << std::endl;
  }
}

int tid = 0;
int tightloop(void* arg)
{
  tid = syscall(SYS_gettid);
  //long ret = ptrace (PTRACE_TRACEME, 0, nullptr, nullptr);
   //std::cout << "traceme ret=" << ret << std::endl;
//  pid = gettid();
  do_log<0,0> start;
  for (int i=0; ;i++) {
    //std::cout << "Iteration " << i << std::endl;
    start.log(nullptr);
  }
  return 0;//nullptr;
}



// ./include/tdep-x86_64/libunwind_i.h:228:#define tdep_get_as(c)                  ((c)->dwarf.as)
// ./include/tdep-x86_64/libunwind_i.h:229:#define tdep_get_as_arg(c)              ((c)->dwarf.as_arg)



bool b = false;
unw_cursor_t cursor2;

void show_backtrace (uint64_t rip, uint64_t rbp, uint64_t rsp) {
#if 0
  void *buffer[100];
  int nptrs;
    nptrs = backtrace(buffer, 100);
//    backtrace_symbols_fd(buffer, nptrs, 2);
    for (int i=0;i<nptrs;i++)
    {
      unw_word_t diff;
      char znaczki[100];
      unw_set_reg(&cursor2, UNW_REG_IP, (unw_word_t)buffer[i]);
        //unw_set_reg(&cursor2, UNW_REG_SP, sp);
      unw_get_proc_name(&cursor2, znaczki, 99, &diff);
          printf ("#%d ip = %lx, [%s+%ld]\n", i, (long) (unw_word_t)buffer[i], znaczki, diff);

    }
#endif
  if (!b) {
    unw_context_t uc;
    unw_getcontext(&uc);
    unw_init_local(&cursor2, &uc);
    b = true;
  }
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip, sp;
  //printf("cursoe = %ld\n",sizeof(cursor.dwarf.as_arg));
  //uint64_t start = now();
  unw_getcontext(&uc);
  uc.uc_mcontext.gregs[REG_RBP] = rbp;
  uc.uc_mcontext.gregs[REG_RIP] = rip;
  uc.uc_mcontext.gregs[REG_RSP] = rsp;

  unw_init_local(&cursor, &uc);
  char buffer[100];
  unw_word_t diff;
  unw_get_proc_name(&cursor, buffer, 99, &diff);
  printf ("#%2d ip = %lx, sp = %lx [%s+%ld]\n", -1, (long) rip, (long) rsp, buffer, diff);
  int i=0 ;
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);
    void* addr = (void*)ip;

    {
      unw_cursor_t cursor;
        unw_context_t uc;
        unw_getcontext(&uc);
      uc.uc_mcontext.gregs[REG_RBP] = 0;//rbp;
      uc.uc_mcontext.gregs[REG_RIP] = ip;
      uc.uc_mcontext.gregs[REG_RSP] = 0;//rsp;

      unw_init_local(&cursor, &uc);
    //unw_set_reg(&cursor2, UNW_REG_IP, ip);
    //unw_set_reg(&cursor2, UNW_REG_SP, sp);
    unw_get_proc_name(&cursor, buffer, 99, &diff);
    printf ("#%2d ip = %lx, sp = %lx [%s+%ld]\n", i, (long) ip, (long) sp, buffer, diff);
    }
    i++;
  }
  //uint64_t end = now();
}

static constexpr size_t ip_table_size = 4096;
uint64_t ip_table[ip_table_size];
std::atomic<size_t> produced{0};
size_t consumed{0};
sem_t process;

struct conveyor
{
public:
  conveyor(size_t size) : size(size)
  {
    ip_table = new uint64_t[size];
  }

  inline uint64_t* produce_pos()
  {
    return ip_table + (produced.load() % size);
  }
  inline size_t produce_avail()
  {
    return size - (produced.load() - consumed);
  }

  inline uint64_t* consume_pos()
  {
    return ip_table + (consumed % size);
  }
  inline size_t consume_avail()
  {
    return produced.load() - consumed;
  }

  inline uint64_t* next(uint64_t* pos)
  {
    pos++;
    if (pos == ip_table + size)
      pos = ip_table;
    return pos;
  }

  inline uint64_t* prev(uint64_t* pos)
  {
    if (pos == ip_table)
      pos = ip_table + (size - 1);
    else
      pos--;
    return pos;
  }

  void inline advance(size_t how_much)
  {
    produced += how_much;
  }

//private:
  size_t size{4096};
  std::atomic<size_t> produced{0};
  size_t consumed{0};
  sem_t notify;
  uint64_t* ip_table;
};

struct conveyor conv(4096);

void get_backtrace(uint64_t rip, uint64_t rbp, uint64_t rsp, uint64_t marker)
{
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip, sp;
  unw_getcontext(&uc);
  uc.uc_mcontext.gregs[REG_RBP] = rbp;
  uc.uc_mcontext.gregs[REG_RIP] = rip+1;
  uc.uc_mcontext.gregs[REG_RSP] = rsp;

  if (rip - (uint64_t)&rand <16)
    {
      printf("\n----- close enough %lx\n",rip);
    }
  size_t count = 0;
  uint64_t* pos = conv.produce_pos();
  size_t avail = conv.produce_avail();
  if (avail <= 1) {
    return;
  }
  *pos = rip;
  pos = conv.next(pos);
  count++;
  avail--;
  unw_init_local(&cursor, &uc);
  bool b=!(rip&0x03f);
  uint64_t rbp_arch;
  uint64_t rsp_arch;
  while (unw_step(&cursor) > 0)
  {
    unw_get_reg(&cursor, UNW_REG_SP, &rsp_arch);

    if (avail == 0) {
      return;
    }
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    if (ip>0x800000000000L){
      printf("#%ld (%d) XXXXX = %lx SP=%lx rsp=%lx rip=%lx rand=%lx\n", count, marker, ip,rsp_arch,rsp,rip, &rand);
      printf("XXXXX rsp=%lx rbp=%lx *sp=%lx *(rbp+8)=%lx\n", rsp, rbp, *(uint64_t*)(rsp), *(uint64_t*)(rbp+8));
      //unw_set_reg(&cursor, UNW_REG_IP, ip>>8);
//      b=true;
      {
        unw_cursor_t cursor1;
        unw_init_local(&cursor1, &uc);
        while (unw_step(&cursor1) > 0)
        {
          unw_word_t ip1,sp1;
          unw_get_reg(&cursor1, UNW_REG_IP, &ip1);
          unw_get_reg(&cursor1, UNW_REG_SP, &sp1);
          printf(">>>> sp=%lx ip=%lx\n",sp1, ip1);
        }
      }
    }
    if (b){
     // printf("#%d (%d) _____ = %lx SP=%lx rsp=%lx\n", count, marker, ip,rsp_arch,rsp);
      //unw_set_reg(&cursor, UNW_REG_IP, ip>>8);
//      b=true;
    }
    *pos = ip;
    pos = conv.next(pos);
    count++;
    avail--;
  }
  //if(!(rip&0xfff))
  //  printf("#%d (%d) ____ = %lx SP=%lx rsp=%lx\n", count, marker, ip,rsp_arch,rsp);

  conv.advance(count);
  if (avail < conv.size/2) {
    //make sure reader is notified
    sem_trywait(&conv.notify);
    sem_post(&conv.notify);
  }
}



void peek_results_old()
{
  printf("conv_pr= %p\n",&conv);
  printf("peek results prod=%p cons=%p prod_free=%d cons_free=%d\n",
         conv.produce_pos(), conv.consume_pos(), conv.produce_avail(), conv.consume_avail());
  if (sem_trywait(&conv.notify) == 0)
  {
    printf("NOTIFIED\n");
    //notified.
    uint64_t* pos = conv.consume_pos();

    size_t avail = conv.consume_avail();
    size_t avail_copy = avail;

    unw_cursor_t cursor;
    unw_context_t uc;
    unw_getcontext(&uc);
    uc.uc_mcontext.gregs[REG_RBP] = 0;//rbp;
    uc.uc_mcontext.gregs[REG_RSP] = 0;//rsp;
    unw_init_local(&cursor, &uc);

    while (avail > 0)
    {
      char buffer[100];
      unw_word_t diff;
      unw_set_reg(&cursor, UNW_REG_IP, *pos);
      //unw_set_reg(&cursor2, UNW_REG_SP, sp);
      unw_get_proc_name(&cursor, buffer, 99, &diff);
      printf ("# ip = %lx, [%s+%ld]\n", (long) *pos, buffer, diff);

      std::string name;
      uint64_t diff1;
      std::tie(name, diff1) = callstep::get_symbol(*pos);
      printf ("# ip = %lx, [%s+%ld]\n", (long) *pos, name.c_str(), diff1);

      //printf ("# ip = %lx, \n", (long) *pos);
      pos = conv.next(pos);
      avail--;

    }
    conv.consumed += avail_copy;
  }
}

callstep* root = nullptr;
void peek_results()
{
  printf("conv_pr= %p\n",&conv);
  printf("peek results prod=%p cons=%p prod_free=%d cons_free=%d\n",
         conv.produce_pos(), conv.consume_pos(), conv.produce_avail(), conv.consume_avail());
  if (sem_trywait(&conv.notify) == 0)
  {
    printf("NOTIFIED\n");
    //notified.

    uint64_t* pos = conv.consume_pos();

    size_t avail = conv.consume_avail();
    size_t avail_copy = avail;
/*
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_getcontext(&uc);
    uc.uc_mcontext.gregs[REG_RBP] = 0;//rbp;
    uc.uc_mcontext.gregs[REG_RSP] = 0;//rsp;
    unw_init_local(&cursor, &uc);
*/
    if (root == nullptr)
    {
      root = new callstep(std::string(), 0);
      //static callstep root(std::string(), 0);
    }

    while (avail > 0)
    {
      uint64_t* pos_i = pos;
      size_t count = 0;
      while (*pos_i != 0)
      {
        pos_i = conv.next(pos_i);
        count++;
      }
      pos = pos_i;
      pos_i = conv.prev(pos_i);
      callstep* node = root;
      while (count > 0)
      {
        node->hit_count++;
        //printf ("# ip = %lx, [%s] base=%lx cnt=%ld\n", (long) *pos_i, node->name.c_str(), node->base_addr, node->hit_count);
        node = node->find_function(*pos_i);
        pos_i = conv.prev(pos_i);
        count --;
      }
      node->hit_count++;

      //std::string name;
      //uint64_t diff1;
      //std::tie(name, diff1) = callstep::get_symbol(*pos);
     // printf ("# ip = %lx, [%s+%ld]\n", (long) *pos, name.c_str(), diff1);

      //printf ("# ip = %lx, \n", (long) *pos);
      pos = conv.next(pos);
      avail--;

    }
    conv.consumed += avail_copy;
  }
}




#define BT_BUF_SIZE 100

extern "C" void grab_callstack(void);
extern "C" void my_backtrace(uint64_t rip, uint64_t rbp, uint64_t rsp, uint64_t marker);


std::atomic<bool> my_lock{false};
void my_backtrace(uint64_t rip, uint64_t rbp, uint64_t rsp, uint64_t marker)
{
  if(my_lock.exchange(true) == false)
  {
    //printf("Hello rip=%lx rbp=%lx rsp=%lx\n",rip, rbp, rsp);
    //show_backtrace(rip, rbp, rsp);
    get_backtrace(rip, rbp, rsp, marker);

    assert(my_lock.exchange(false) == true);
  }
}

extern "C"
void _wrapper(void);

extern "C"
void _wrapper_regs_provided(void);

extern "C"
void _wrapper_to_func(void);



void printfunc(uint64_t rip)
{
  char buffer[100];
  unw_word_t diff;
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_getcontext(&uc);
  uc.uc_mcontext.gregs[REG_RBP] = 0;//rbp;
  uc.uc_mcontext.gregs[REG_RIP] = rip;
  uc.uc_mcontext.gregs[REG_RSP] = 0;//rsp;

  unw_init_local(&cursor, &uc);
  //unw_set_reg(&cursor2, UNW_REG_IP, ip);
  //unw_set_reg(&cursor2, UNW_REG_SP, sp);
  unw_get_proc_name(&cursor, buffer, 99, &diff);
  printf ("FUNC ip = %lx, [%s+%ld]\n", (long) rip, buffer, diff);

}







void probe_orig(int target_pid)
{
  long ret = ptrace (PTRACE_SEIZE, target_pid, nullptr, nullptr);
  std::cout << "seize ret=" << ret << std::endl;
  sleep(1);
  ret = ptrace(PTRACE_SETOPTIONS, target_pid, nullptr, 0); //PTRACE_O_TRACESYSGOOD);
  ret = ptrace(PTRACE_SETOPTIONS, target_pid, 0,
                              PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK);
  std::cout << "setoptions ret=" << ret << std::endl;

  {
  ret = ptrace (PTRACE_INTERRUPT, target_pid, NULL, 0);
  std::cout << "interrupt ret=" << ret << std::endl;

  int wstatus;
  pid_t ppp = waitpid(target_pid, &wstatus, 0);

  ret = ptrace(PTRACE_SETOPTIONS, target_pid, 0,
               PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACESYSGOOD);
  std::cout << "setoptions ret=" << ret << std::endl;

  ret = ptrace (PTRACE_CONT, target_pid, NULL, 0);
  std::cout << "cont ret=" << ret << std::endl;

  }
  user_regs_struct regs;
  bool doprint = false;
  bool dograb = false;
  for (int i=0;i<1000000;i++)
  {
    xxx:
    long ret = ptrace (PTRACE_INTERRUPT, target_pid, NULL, 0);
    //std::cout << "interrupt ret=" << ret << std::endl;

    int wstatus;
    zzz:
    pid_t ppp = waitpid(target_pid, &wstatus, 0);
    //std::cout << "wstatus cause=" << WIFSTOPPED(wstatus) << " event=" << std::hex << (wstatus >> 8 ) << std::dec <<std::endl;
    //std::cout << "wexitstatus=" << WEXITSTATUS(wstatus) << std::endl;


    if((i%1000 == 0) || doprint)
    {
      std::cout << "it " << i << std::hex <<
          " WSTOPSIG=" << WSTOPSIG(wstatus) <<
          " WIFSTOPPED=" << WIFSTOPPED(wstatus) << std::dec<<
          std::endl;
      //std::cout << std::hex << "last.rip=" << last_rip << std::dec << std::endl;
      //std::cout << "eflags=" << ((regs.eflags >> 12 ) &3 ) << std::endl;
      doprint=false;
    }


#if 1
    if (WIFSTOPPED(wstatus)) // && WSTOPSIG(wstatus) == SIGTRAP )
    {
      //std::cout << "wstopsig = " << WSTOPSIG(wstatus) << std::endl;
      //sleep(1);

    }
#endif



#if 0
    if (WSTOPSIG(wstatus) != SIGTRAP) {
        std::cout << "wstopsig cause=" << WSTOPSIG(wstatus) << std::endl;
        std::cout << "eflags=" << ((regs.eflags >> 12 ) &3 ) << std::endl;
        goto yyy;
    }
#endif
    //ret = ptrace (PTRACE_INTERRUPT, target_pid, NULL, 0);
    //std::cout << "interrupt ret=" << ret << std::endl;

    //std::cout << "event = " << (event >> 8) << std::endl;
    //}

    ret = ptrace (PTRACE_GETREGS, target_pid, NULL, &regs);
    assert (ret == 0);
    if (ret != 0 ) {
      //perror("failed to get regs");
      ret = ptrace (PTRACE_CONT, target_pid, NULL, 0);
      //std::cout << "cont ret=" << ret << std::endl;
      goto zzz;
    }

    //std::cout << "ip = " << std::hex << regs.rip << std::dec << std::endl;
    //std::cout << "event = " << std::hex << (wstatus >> 16) << std::dec << std::endl;

    {
      int event = wstatus >> 8 >> 8;
      if (event != SI_KERNEL)
      {
        std::cout << "SI_KERNEL" << std::endl;

        //sleep(10);
      }
    }
    if (*(uint8_t*)(regs.rip-2) == 0x0f &&
        *(uint8_t*)(regs.rip-1) == 0x05)
    {
      std::cout << "syscall condition reached ip=" << std::hex << regs.rip << std::dec << " i="<< i << std::endl;
      printfunc(regs.rip);

      siginfo_t sig;
      ret = ptrace (PTRACE_GETSIGINFO, target_pid, nullptr, &sig);
      if(0)
      std::cout << "ptrace getsiginfo=" << ret << " si_signo=" << sig.si_signo <<
          " si_code=" << std::hex << sig.si_code << std::dec << std::endl;
//      int      si_signo;     /* Signal number */
//                   int      si_errno;
      int event = wstatus >> 8 >> 8;
      if (event == SI_KERNEL)
      {
        std::cout << "cowardly refusing to meddle with syscall" << std::endl;
        std::cout << "wstatus" << std::hex << wstatus << std::dec << std::endl;
        ret = ptrace (PTRACE_SYSCALL, target_pid, NULL, 0);
        std::cout << "ptrace syscall=" << ret << std::endl;
        std::cout << "it #" << i << std::endl;
        doprint = true;
#if 0
        ret = ptrace (PTRACE_CONT, target_pid, NULL, 0);
        std::cout << "cont ret=" << ret << std::endl;
        ret = ptrace (PTRACE_INTERRUPT, target_pid, NULL, 0);
        std::cout << "interrupt ret=" << ret << std::endl;
#endif
        //abort();
        //goto yyy1;
        goto zzz;
      }
      else
      {

        //ret = ptrace (PTRACE_SINGLESTEP, target_pid, NULL, 0);
        //std::cout << "ptrace singlestep=" << ret << std::endl;
        doprint = true;
        dograb = true;
        goto dodo;
      }

    }
    dodo:
    if (rand()%1000 == 0)
      std::cout << "wstatus" << std::hex << wstatus << std::dec << std::endl;

    if(0)
    std::cout << std::hex <<
        "##    RIP=" <<  (uint64_t)regs.rip <<
        " RBP=" <<  (uint64_t)regs.rbp <<
        " RSP=" <<  (uint64_t)regs.rsp <<
        std::dec << std::endl;


    if (dograb)
    {
      std::cout << "DOGRAB" << std::endl;
      dograb = false;

    }
    if (regs.rip != (uint64_t)_wrapper)
    {
      regs.rsp -= ( 128 + 8);
      ret = ptrace (PTRACE_POKEDATA, target_pid, (uint64_t*)(regs.rsp), (void*)regs.rip);

      regs.rip = (uint64_t)_wrapper;

      ret = ptrace (PTRACE_SETREGS, target_pid, NULL, &regs);
    }
    else
    {
      std::cout << "RIP == wrapper" << std::endl;
      ret = ptrace (PTRACE_CONT, target_pid, NULL, 0);
      std::cout << "cont ret=" << ret << std::endl;
      usleep(10*1000);
      continue;
    }
    //std::cout << "setregs ret=" << ret << std::endl;

    //sleep(1);

    yyy:
    //while (stopped_count--) {
      ret = ptrace (PTRACE_CONT, target_pid, NULL, 0);
      //std::cout << "cont ret=" << ret << std::endl;
      //assert(ret == 0);
      //usleep(1);
    //}

    yyy1:
    usleep(10*1000);
    //ret = ptrace (PTRACE_CONT, target_pid, NULL, 0);
    //std::cout << "cont ret=" << ret << std::endl;
    //assert (ret == 0);
    //usleep(10*1000);
  }

}





typedef void interruption_func(void);
//void

/*
 *
 */
int setup_execution_frame(user_regs_struct& regs,
                           interruption_func func,
                           pid_t pid)
{
  //make frame big enough to skip "scratch area"
  //x86_64-abi section 3.2.2 The Stack Frame, declares sp+0 .. sp+128 as reserved
  int ret;
  regs.rsp -= ( 128 + 8);
  ret = ptrace(PTRACE_POKEDATA, pid, (uint64_t*)(regs.rsp), (void*)regs.rip);
  regs.rip = (uint64_t)func;
  if (ret == 0)
    ret = ptrace(PTRACE_SETREGS, pid, nullptr, &regs);
  return ret;
}


int setup_execution_frame(user_regs_struct& regs,
                           const user_regs_struct& previous_regs,
                           interruption_func func,
                           pid_t pid)
{
  int ret;
  regs.rsp -= ( 128 + 8);
  ret = ptrace(PTRACE_POKEDATA, pid, (uint64_t*)(regs.rsp), (void*)previous_regs.rip);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, pid, (uint64_t*)(regs.rsp), (void*)previous_regs.rbp);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, pid, (uint64_t*)(regs.rsp), (void*)previous_regs.rsp);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, pid, (uint64_t*)(regs.rsp), (void*)regs.rip);

  regs.rip = (uint64_t)func;
  if (ret == 0)
    ret = ptrace(PTRACE_SETREGS, pid, nullptr, &regs);
  return ret;
}


void probe_old(int target_pid)
{
  long ret;
  ret = ptrace (PTRACE_SEIZE, target_pid, nullptr, nullptr);
  std::cout << "seize ret=" << ret << std::endl;
  sleep(1);

  user_regs_struct regs;
  int wstatus;
  ret = ptrace(PTRACE_INTERRUPT, target_pid, NULL, 0);
  assert(ret == 0);
  for (int i=0;i<1000000;i++)
  {
    if ((i%100) == 0)
    std::cout << "it " << i << std::endl;
    pid_t ppp = waitpid(target_pid, &wstatus, 0);
    if (WSTOPSIG(wstatus) != SIGTRAP)
    {
      std::cout << "not SIGTRAP " << WSTOPSIG(wstatus) << std::endl;
      goto skip;
    }

    ret = ptrace(PTRACE_GETREGS, target_pid, NULL, &regs);
    assert (ret == 0);

#if 0
    siginfo_t sig;
    ret = ptrace (PTRACE_GETSIGINFO, target_pid, nullptr, &sig);
    if(0)
    std::cout << "ptrace getsiginfo=" << ret << " si_signo=" << sig.si_signo <<
        " si_code=" << std::hex << sig.si_code << std::dec << std::endl;
#endif
    if (*(uint8_t*)(regs.rip-2) == 0x0f &&
        *(uint8_t*)(regs.rip-1) == 0x05)
    {
#if 0
      if(0)
      std::cout << "S" << std::endl;
      if(0)
      std::cout << "call_addr=" << std::hex << sig._sifields._sigsys._call_addr << std::dec
          << " syscall=" << sig._sifields._sigsys._syscall
          << " _arch=" << sig._sifields._sigsys._arch << std::endl;
#endif

      ret = ptrace(PTRACE_SINGLESTEP, target_pid, nullptr, 0);
      assert(ret == 0);
      ppp = waitpid(target_pid, &wstatus, 0);
      user_regs_struct regs_as;

      ret = ptrace(PTRACE_GETREGS, target_pid, nullptr, &regs_as);
      assert (ret == 0);

      if(0)
      std::cout << std::hex <<
          "####  RIP=" <<  (uint64_t)regs.rip <<
          " RBP=" <<  (uint64_t)regs.rbp <<
          " RSP=" <<  (uint64_t)regs.rsp <<
          std::dec << std::endl;

      regs_as.rsp -= ( 128 + 8);
      ret = ptrace(PTRACE_POKEDATA, target_pid, (uint64_t*)(regs_as.rsp), (void*)regs.rip);
      regs_as.rsp -= 8;
      ret = ptrace(PTRACE_POKEDATA, target_pid, (uint64_t*)(regs_as.rsp), (void*)regs.rbp);
      regs_as.rsp -= 8;
      ret = ptrace(PTRACE_POKEDATA, target_pid, (uint64_t*)(regs_as.rsp), (void*)regs.rsp);
      regs_as.rsp -= 8;
      ret = ptrace(PTRACE_POKEDATA, target_pid, (uint64_t*)(regs_as.rsp), (void*)regs_as.rip);

      regs_as.rip = (uint64_t)_wrapper_regs_provided;
      ret = ptrace(PTRACE_SETREGS, target_pid, NULL, &regs_as);


      goto skip;

      int event = wstatus >> 8 >> 8;
      if (event == SI_KERNEL)
      {
        std::cout << "K" << std::endl;
        //it seems we are currently doing syscall
        //lets postpone getting status for moment when we leave
        ret = ptrace(PTRACE_SYSCALL, target_pid, nullptr, 0);
        continue;
      }
    }

    {
    union {
      std::atomic<bool> my_lock;
      uint64_t data;
    } xxx;
    xxx.data = ptrace(PTRACE_PEEKDATA, target_pid, (uint64_t*)(&my_lock), 0);
    if(xxx.my_lock) {
      std::cout << "L" << std::endl;
      goto skip;
    }
    }

    if (regs.rip != (uint64_t)_wrapper)
    {
      action:
      if(0)
      std::cout << std::hex <<
          "##    RIP=" <<  (uint64_t)regs.rip <<
          " RBP=" <<  (uint64_t)regs.rbp <<
          " RSP=" <<  (uint64_t)regs.rsp <<
          std::dec << std::endl;
#if 0
      uint64_t rsp = regs.rsp;
      regs.rsp -= ( 128 + 8);
      ret = ptrace(PTRACE_POKEDATA, target_pid, (uint64_t*)(regs.rsp), (void*)regs.rip);
      regs.rsp -= 8;
      ret = ptrace(PTRACE_POKEDATA, target_pid, (uint64_t*)(regs.rsp), (void*)regs.rbp);
      regs.rsp -= 8;
      ret = ptrace(PTRACE_POKEDATA, target_pid, (uint64_t*)(regs.rsp), (void*)rsp);
      regs.rsp -= 8;
      ret = ptrace(PTRACE_POKEDATA, target_pid, (uint64_t*)(regs.rsp), (void*)regs.rip);

      regs.rip = (uint64_t)_wrapper_regs_provided;
      ret = ptrace(PTRACE_SETREGS, target_pid, NULL, &regs);
#endif
#if 0
      //make frame big enough to skip "scratch area"
      //x86_64-abi section 3.2.2 The Stack Frame, declares sp+0 .. sp+128 as reserved
      regs.rsp -= ( 128 + 8);
      ret = ptrace(PTRACE_POKEDATA, target_pid, (uint64_t*)(regs.rsp), (void*)regs.rip);
      regs.rip = (uint64_t)_wrapper;
      ret = ptrace(PTRACE_SETREGS, target_pid, NULL, &regs);
#endif
      ret = setup_execution_frame(regs,_wrapper, target_pid);
      //ret = ptrace(PTRACE_SETREGS, target_pid, nullptr, &regs);
    }
    else
    {
      static int i=0;
      std::cout << "W" << std::endl;
      i++;
      if(i==1000)
      {
        long ret = ptrace (PTRACE_INTERRUPT, tid, NULL, 0);
        std::cout << "interrupt ret=" << ret << std::endl;

        ret = ptrace (PTRACE_DETACH, tid, NULL, 0);
        std::cout << "detach ret=" << ret << std::endl;
        sleep(3600);
      }
    }
    skip:
    ret = ptrace(PTRACE_CONT, target_pid, NULL, 0);
    assert(ret == 0);
    usleep(1*100);
    ret = ptrace(PTRACE_INTERRUPT, target_pid, NULL, 0);
    assert(ret == 0);
  }

}




void probe_old_2(int target_pid)
{
  long ret;
  ret = ptrace (PTRACE_SEIZE, target_pid, nullptr, nullptr);
  std::cout << "seize ret=" << ret << std::endl;
  sleep(1);

  user_regs_struct regs;
  int wstatus;
  ret = ptrace(PTRACE_INTERRUPT, target_pid, NULL, 0);
  assert(ret == 0);
  for (int i=0;i<1000000;i++)
  {
    if ((i%100) == 0)
    std::cout << "it " << i << std::endl;
    pid_t ppp = waitpid(target_pid, &wstatus, 0);
    if (WSTOPSIG(wstatus) != SIGTRAP)
    {
      std::cout << "not SIGTRAP " << WSTOPSIG(wstatus) << std::endl;
      goto skip;
    }

    ret = ptrace(PTRACE_GETREGS, target_pid, NULL, &regs);
    assert (ret == 0);

    if (*(uint8_t*)(regs.rip-2) == 0x0f &&
        *(uint8_t*)(regs.rip-1) == 0x05)
    {

      ret = ptrace(PTRACE_SINGLESTEP, target_pid, nullptr, 0);
      assert(ret == 0);
      ppp = waitpid(target_pid, &wstatus, 0);

      user_regs_struct regs_as;
      ret = ptrace(PTRACE_GETREGS, target_pid, nullptr, &regs_as);
      assert (ret == 0);
      ret = setup_execution_frame(regs_as, regs, _wrapper_regs_provided, target_pid);
      goto skip;
    }

    if (regs.rip != (uint64_t)_wrapper)
    {
      action:
      ret = setup_execution_frame(regs,_wrapper, target_pid);
    }
    else
    {
      static int i=0;
      std::cout << "W" << std::endl;
      i++;
      if(i==1000)
      {
        long ret = ptrace (PTRACE_INTERRUPT, tid, NULL, 0);
        std::cout << "interrupt ret=" << ret << std::endl;

        ret = ptrace (PTRACE_DETACH, tid, NULL, 0);
        std::cout << "detach ret=" << ret << std::endl;
        sleep(3600);
      }
    }
    skip:
    ret = ptrace(PTRACE_CONT, target_pid, NULL, 0);
    assert(ret == 0);
    usleep(1*100);
    ret = ptrace(PTRACE_INTERRUPT, target_pid, NULL, 0);
    assert(ret == 0);
  }
}





void probe_old_3(int target_pid)
{
  long ret;
  ret = ptrace (PTRACE_SEIZE, target_pid, nullptr, nullptr);
  std::cout << "seize ret=" << ret << std::endl;
  sleep(1);

  user_regs_struct regs;
  int wstatus;
  ret = ptrace(PTRACE_INTERRUPT, target_pid, NULL, 0);
  assert(ret == 0);
  for (int i=0;i<1000000;i++)
  {
    if ((i%100) == 0)
    std::cout << "it " << i << std::endl;
    pid_t ppp = waitpid(target_pid, &wstatus, 0);
    if (WSTOPSIG(wstatus) == SIGTRAP)
    {

      ret = ptrace(PTRACE_GETREGS, target_pid, NULL, &regs);
      assert (ret == 0);

      if (*(uint8_t*)(regs.rip-2) == 0x0f &&
          *(uint8_t*)(regs.rip-1) == 0x05)
      {

        ret = ptrace(PTRACE_SINGLESTEP, target_pid, nullptr, 0);
        assert(ret == 0);
        ppp = waitpid(target_pid, &wstatus, 0);

        user_regs_struct regs_as;
        ret = ptrace(PTRACE_GETREGS, target_pid, nullptr, &regs_as);
        assert (ret == 0);
        ret = setup_execution_frame(regs_as, regs, _wrapper_regs_provided, target_pid);
      }
      else
      {

        if (regs.rip != (uint64_t)_wrapper)
        {
          ret = setup_execution_frame(regs,_wrapper, target_pid);
        }
        else
        {
          static int i=0;
          std::cout << "W" << std::endl;
          i++;
          if(i==1000)
          {
            long ret = ptrace (PTRACE_INTERRUPT, tid, NULL, 0);
            std::cout << "interrupt ret=" << ret << std::endl;

            ret = ptrace (PTRACE_DETACH, tid, NULL, 0);
            std::cout << "detach ret=" << ret << std::endl;
            sleep(3600);
          }
        }
      }
    }
    else
    {
      std::cout << "not SIGTRAP " << WSTOPSIG(wstatus) << std::endl;
    }


    ret = ptrace(PTRACE_CONT, target_pid, NULL, 0);
    assert(ret == 0);
    usleep(1*100);
    ret = ptrace(PTRACE_INTERRUPT, target_pid, NULL, 0);
    assert(ret == 0);
  }
}






class probe_thread
{
public:
  probe_thread(): m_target(0) {}
  ~probe_thread() {}

private:
  pid_t m_target{0};
  bool m_retry{false};
  user_regs_struct regs;
  /*
   *
   */
  int setup_execution_frame(user_regs_struct& regs)
  {
    //make frame big enough to skip "scratch area"
    //x86_64-abi section 3.2.2 The Stack Frame, declares sp+0 .. sp+128 as reserved
    int ret;
    regs.rsp -= ( 128 + 8);
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)regs.rip);
    regs.rip = (uint64_t)_wrapper;
    if (ret == 0)
      ret = ptrace(PTRACE_SETREGS, m_target, nullptr, &regs);
    return ret;
  }

  int setup_execution_frame(user_regs_struct& regs,
                            const user_regs_struct& previous_regs)
  {
    int ret;
    regs.rsp -= ( 128 + 8);
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)previous_regs.rip);
    regs.rsp -= 8;
    if (ret == 0)
      ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)previous_regs.rbp);
    regs.rsp -= 8;
    if (ret == 0)
      ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)previous_regs.rsp);
    regs.rsp -= 8;
    if (ret == 0)
      ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)regs.rip);

    regs.rip = (uint64_t)_wrapper_regs_provided;
    if (ret == 0)
      ret = ptrace(PTRACE_SETREGS, m_target, nullptr, &regs);
    return ret;
  }
public:
  int setup_execution_func(user_regs_struct& regs,
                            interruption_func func,
                            uint64_t arg1 = 0,
                            uint64_t arg2 = 0,
                            uint64_t arg3 = 0)
  {
    int ret;
    regs.rsp -= ( 128 + 8);
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)func);
    regs.rsp -= 8;
    if (ret == 0)
      ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)arg1);
    regs.rsp -= 8;
    if (ret == 0)
      ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)arg2);
    regs.rsp -= 8;
    if (ret == 0)
      ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)arg3);
    regs.rsp -= 8;
    if (ret == 0)
      ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)regs.rip);

    regs.rip = (uint64_t)_wrapper_to_func;
    if (ret == 0)
      ret = ptrace(PTRACE_SETREGS, m_target, nullptr, &regs);
    return ret;
  }


public:
  bool seize(pid_t target)
  {
    if (m_target != 0)
      detach();
    m_target = target;
    long ret;
    ret = ptrace(PTRACE_SEIZE, m_target, nullptr, nullptr);
    return (ret == 0);
  }

  bool detach()
  {
    long ret;
    ret = ptrace(PTRACE_INTERRUPT, m_target, nullptr, nullptr);
    if (ret == 0)
    {
      waitpid(m_target, nullptr, 0);
      ret = ptrace(PTRACE_DETACH, m_target, nullptr, nullptr);
    }
    m_target = 0;
    return (ret == 0);
  }

  bool signal_interrupt()
  {
    long ret;
    ret = ptrace(PTRACE_INTERRUPT, m_target, nullptr, nullptr);
    return (ret == 0);
  }

  bool cont()
  {
    long ret;
    ret = ptrace(PTRACE_CONT, m_target, nullptr, nullptr);
    return (ret == 0);
  }


  bool grab_callback()
  {
    long ret;
    ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
    if (ret != 0)
      return false;
    errno = 0;
    uint64_t code = ptrace(PTRACE_PEEKDATA, m_target, (uint64_t*)(regs.rip-2), nullptr);
    if (errno != 0)
      return false;
    if ((code & 0xffff ) == 0x050f)   //0x0f05 is SYSCALL
    {
      ret = ptrace(PTRACE_SINGLESTEP, m_target, nullptr, nullptr);
      if (ret == 0)
      {
        int wstatus;
        waitpid(m_target, &wstatus, 0);
        user_regs_struct regs_as;
        ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs_as);
        if (ret == 0)
        {
          ret = setup_execution_frame(regs_as, regs);
        }
      }
    }
    else
    {
      ret = 0;
      if (regs.rip != (uint64_t)_wrapper &&
          regs.rip != (uint64_t)_wrapper_regs_provided)
      {
        ret = setup_execution_frame(regs);
      }
    }
    if (ret == 0)
    {
      if (!cont())
      {
        ret = -1;
      }
    }
    return (ret == 0);
  }

};
extern "C"
void _test_do_print(void* cs,void* a, void* b);
void _test_do_print(void* cs,void* a, void* b)
{
  printf("test_do_print %p %p %p\n",cs, a, b);
}

void do_print(callstep** cs)
{
  printf("xxxx %p\n",cs);
  (*cs)->print(0, std::cout);
}

bool probe(int target_pid)
{
  long ret;

  probe_thread pt;

  if (! pt.seize(target_pid))
    return false;
  sleep(1);

  user_regs_struct regs;
  int wstatus;

  if (! pt.signal_interrupt())
    return false;

  for (int i=0;i<100000;i++)
  {
    if ((i%100) == 0)
    std::cout << "it " << i << std::endl;
    pid_t ppp = waitpid(target_pid, &wstatus, 0);
    if (WSTOPSIG(wstatus) == SIGTRAP)
    {
      bool b;
      b = pt.grab_callback();
      assert(b);
    }
    else
    {
      std::cout << "not SIGTRAP " << WSTOPSIG(wstatus) << std::endl;
    }
    //ret = ptrace(PTRACE_CONT, target_pid, NULL, 0);
    //assert(ret == 0);
    usleep(1*100);
    ret = ptrace(PTRACE_INTERRUPT, target_pid, NULL, 0);
    assert(ret == 0);
  }

  waitpid(target_pid, nullptr, 0);
  ptrace(PTRACE_SINGLESTEP, target_pid, nullptr, nullptr);
  waitpid(target_pid, nullptr, 0);



  std::cout << "finished" << std::endl;
  ret = ptrace(PTRACE_GETREGS, target_pid, nullptr, &regs);
  if (ret != 0)
    return false;
  std::cout << "finished 1" << std::endl;
  pt.setup_execution_func(regs, (interruption_func*)do_print, (uint64_t)&root);
  pt.cont();
  std::cout << "finished 2 " << &root << std::endl;
  sleep(1);
  std::cout << "finished 3" << std::endl;

}







int static_tid = 0;
void pppp()
{
  int tid = static_tid;
  long ret = ptrace (PTRACE_INTERRUPT, tid, NULL, 0);
  std::cout << "interrupt ret=" << ret << std::endl;

  ret = ptrace (PTRACE_DETACH, tid, NULL, 0);
  std::cout << "detach ret=" << ret << std::endl;
}








#define STACK_SIZE (1024 * 1024)


int main(int argc, char** argv)
{


  std::cout << "res=" << _rax_test(100000) << std::endl;
#if 1
 //_wrapper();
// my_backtrace(100);
  pthread_t thr;
  char *vstack = (char*)malloc(STACK_SIZE);
  pid_t v;
  if (clone(tightloop, vstack + STACK_SIZE, CLONE_PARENT_SETTID | CLONE_FILES | CLONE_FS | CLONE_IO | CLONE_VM, NULL, &v) == -1) { // you'll want to check these flags
    perror("failed to spawn child task");
    return 3;
  }
  //int r=pthread_create(&thr, nullptr, tightloop, nullptr);
  //printf("pthread_create=%d\n",r);
  sleep(100000);

#endif
  //int pid = atoi(argv[1]);

  //std::cout << "TID=" << tid << std::endl;
  tid = v;
  //tid=pid;
  std::cout << "TID=" << tid << std::endl;
  static_tid = 0;
  probe(tid);
  long ret = ptrace (PTRACE_INTERRUPT, tid, NULL, 0);
  std::cout << "interrupt ret=" << ret << std::endl;
  waitpid(tid, nullptr, 0);



  ret =
  ptrace (PTRACE_DETACH, tid, NULL, 0);
  std::cout << "detach ret=" << ret << std::endl;
  //kill(tid, 15);
  sleep(10000);
}
