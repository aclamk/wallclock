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

#include "manager.h"
#include "agent.h"
static int iiii=1111;

#include "loader.h"

/*
uint64_t R_init_agent_call = 0;
uint64_t R_create_sampling_context_call = 0;
uint64_t R_print_peek_call = 0;

uint64_t _wrapper_call = 0;
uint64_t _wrapper_regs_provided_call = 0;
uint64_t _wrapper_to_func_call = 0;
*/


void* locate_library(pid_t pid, const std::string& library_name);

void init_agent_interface(pid_t remote)
{
  void* my_agent_so = locate_library(syscall(SYS_gettid), "agent.so");
  void* remote_agent_so = locate_library(remote, "agent.so");

  printf("my=%p remote=%p\n", my_agent_so, remote_agent_so);
  int64_t diff = (uint64_t)remote_agent_so - (uint64_t)my_agent_so;

  printf("R_init_agent_call agent_interface=%p\n",&agent_interface);
  R_init_agent_call = (uint64_t)agent_interface[1] + diff;
  R_create_sampling_context_call = (uint64_t)agent_interface[2] + diff;
  R_print_peek_call = (uint64_t)agent_interface[3] + diff;

  _wrapper_call = (uint64_t)agent_interface[4] + diff;
  _wrapper_regs_provided_call = (uint64_t)agent_interface[5] + diff;
  _wrapper_to_func_call = (uint64_t)agent_interface[6] + diff;

  printf("R_init_agent_call=%lx\n", R_init_agent_call);
  printf("R_create_sampling_context_call=%lx\n", R_create_sampling_context_call);
  printf("R_print_peek_call=%lx\n", R_print_peek_call);
}

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
  if ((depth%2) && (rand() % 2)) {
  //if (rand() % 2) {
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
  if ((i % 160000) == 0) {

    std::cerr << "-------" << now() << "--------------End " << recursion(cct) << "x=" << x << " stack=" << std::hex << &i << std::dec << std::endl;
    //peek_results();

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









extern "C" void grab_callstack(void);
extern "C" void my_backtrace(uint64_t rip, uint64_t rbp, uint64_t rsp);


//std::atomic<bool> my_lock{false};

#if 0
void my_backtrace(uint64_t rip, uint64_t rbp, uint64_t rsp)
{
  if(my_lock.exchange(true) == false)
  {
    //printf("Hello rip=%lx rbp=%lx rsp=%lx\n",rip, rbp, rsp);
    //show_backtrace(rip, rbp, rsp);
    get_backtrace(rip, rbp, rsp);

    assert(my_lock.exchange(false) == true);
  }
}
#endif
extern "C"
void _wrapper(void);

extern "C"
void _wrapper_regs_provided(void);

extern "C"
void _wrapper_to_func(void);














extern "C"
void _remote_return(uint64_t a, uint64_t b, uint64_t c);
//void _test_do_print(void* cs,void* a, void* b)

extern "C"
void _test_do_print(void* cs,void* a, void* b);
void _test_do_print(void* cs,void* a, void* b)
{

  iiii++;
  printf("test_do_print %p %p %p %d\n\n\n\n\n",cs, a, b,iiii);
  //sleep(1);
  _remote_return(11,22,33);
  //printf("return from test_do_print\n");
  //sleep(10);
}

void do_print(callstep** cs)
{
  printf("xxxx %p\n",cs);
  (*cs)->print(0, std::cout);
}

//void R_create_sampling_context();


bool probe(int target_pid)
{
  long ret;

  probe_thread pt;

  if (! pt.seize(target_pid))
    return false;
  sleep(1);
  user_regs_struct regs;
  int wstatus;
//  pt.setup_execution_func(pt.regs, (interruption_func*)_test_do_print,1,2,7);
//  pt.wait_return();

  if (! pt.signal_interrupt())
    return false;
  if (!pt.wait_stop(wstatus))
    return false;
  if (!pt.read_regs())
      return false;
  pt.setup_execution_func(pt.regs, (interruption_func*)R_init_agent_call);
  pt.cont();
  //uint64_t context;
  //pt.wait_return(&context);
  sleep(1);
  printf("R1\n");
  if (! pt.signal_interrupt())
    return false;
  printf("R1.1\n");

  if (!pt.wait_stop(wstatus))
    return false;
  printf("R1.2\n");
  if (!pt.read_regs())
    return false;
  printf("R1.3\n");
  pt.setup_execution_func(pt.regs, (interruption_func*)R_create_sampling_context_call);
  uint64_t context;
  pt.wait_return(&context);
  pt.set_remote_context(context);
  printf("R2\n");


  if (! pt.signal_interrupt())
    return false;

  for (int i=0;i<1000;i++)
  {
    if ((i%1) == 0)
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
    usleep(10*1000);
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
  //pt.setup_execution_func(regs, (interruption_func*)do_print, (uint64_t)&root);
  pt.cont();
  //std::cout << "finished 2 " << &root << std::endl;
  sleep(1);
  std::cout << "finished 3" << std::endl;


  printf("Z1\n");
  if (! pt.signal_interrupt())
    return false;
  printf("Z1.1\n");

  if (!pt.wait_stop(wstatus))
    return false;
  printf("Z1.2\n");
  if (!pt.read_regs())
    return false;
  printf("Z1.3\n");
  pt.setup_execution_func(pt.regs, (interruption_func*)R_print_peek_call, context);
  //uint64_t context;
  printf("Z2\n");
  pt.cont();

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
void* locate_library(pid_t pid, const std::string& library_name);

int main(int argc, char** argv)
{

  pid_t v;

#if 1
 //_wrapper();

  // my_backtrace(100);
  if (argc < 2)
  {
    pthread_t thr;
    char *vstack = (char*)malloc(STACK_SIZE);
    if (clone(tightloop, vstack + STACK_SIZE, CLONE_PARENT_SETTID | CLONE_FILES | CLONE_FS | CLONE_IO /*| CLONE_VM*/, NULL, &v) == -1) { // you'll want to check these flags
      perror("failed to spawn child task");
      return 3;
    }
    //int r=pthread_create(&thr, nullptr, tightloop, nullptr);
    //printf("pthread_create=%d\n",r);
    sleep(1);
  }
  else
  {
    v = atoi(argv[1]);
    printf("REMOTE_TID=%d\n",v);
  }

#endif

  printf("ADD=%lx\n", (uint64_t)locate_library(v,"agent.so"));

  init_agent_interface(v);
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

  //std::cout << "backtrace_counter=" << backtrace_counter.load() << std::endl;
  //kill(tid, 15);
  sleep(10000);
}
