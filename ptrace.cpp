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
#include <sys/time.h>

       #include <sys/types.h>
       #include <sys/time.h>
       #include <sys/resource.h>
       #include <sys/wait.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>


#include "callstep.h"

#include "manager.h"
#include "agent.h"
#include "loader.h"


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


















extern "C"
void _remote_return(uint64_t a, uint64_t b, uint64_t c);





monitored_thread* currently_interrupted_thread{0};
std::atomic<uint32_t> wait_alarm_invoked{0};
void wait_alarm_signal(int)
{
  wait_alarm_invoked++;
  monitored_thread* mt = currently_interrupted_thread;
  if (mt)
    mt->signal_interrupt();
}

std::vector<pid_t> tids;
std::vector<monitored_thread> pts;
std::atomic<uint32_t> backtrace_remains{0};
sem_t backtrace_grab_finished;
void child_func(int signo, siginfo_t * info, void * ctx)
{
  printf("signo=%d si_signo=%d si_code=%d si_pid=%d\n", signo, info->si_signo, info->si_code, info->si_pid);
  if (info->si_code !=CLD_TRAPPED) return;
  return;
  size_t i;
  for (i=0; i <tids.size(); i++)
  {
    if (tids[i] == info->si_pid) {
      pts[i].grab_callback();
      if( backtrace_remains.fetch_add(-1) == 1) {
        sem_post(&backtrace_grab_finished);
      }
      break;
    }
  }
  if (i == tids.size())
  {
    printf("unexpected child pid=%d\n",info->si_pid);
  }
}




bool probe2(Manager& mgr)
{
  bool res = true;
  long ret;
  for (size_t i=0; i<tids.size(); i++)
  {
    if (res) res = mgr.trace_attach(tids[i]);
  }
  uint64_t time = 0;

  for (int iter=0;iter<1000;iter++)
  {
    uint64_t start = -now();
    res = true;
    if ((iter%100) == 0)
      std::cout << "it " << iter << "time=" << time/(double)1000000/iter << "ms " << std::endl;
    res = mgr.probe();
    start += now();
    if (start > 10*1000*1000) {
      std::cout << "iteration took " << start << " ns" << std::endl;
    }
    time += start;
    //usleep(20*1000);
  }
  for (size_t i=0; i<tids.size(); i++)
  {
    mgr.dump_tree(tids[i]);
  }

}






int main(int argc, char** argv)
{
  //std::vector<pid_t> tids;
  pid_t v;
#if 1
 //_wrapper();

  // my_backtrace(100);
  if (argc < 2)
  {
    std::cerr << "PID not provided" << std::endl;
    exit(-1);
  }
  else
  {
    for (int i = 1; i < argc ; i++)
    {
      v = atoi(argv[i]);
      printf("REMOTE_TID=%d\n",v);
      tids.push_back(v);
    }
  }

#endif

  Manager mgr;
  init_agent_interface(mgr, v);
  //int pid = atoi(argv[1]);

  //std::cout << "TID=" << tid << std::endl;
  tid = v;
  //tid=pid;
  std::cout << "TID=" << tid << std::endl;


  //mgr.trace_attach(v);
  //sleep(10);
  probe2(mgr);


  //std::cout << "backtrace_counter=" << backtrace_counter.load() << std::endl;
  //kill(tid, 15);
  sleep(10000);
}
