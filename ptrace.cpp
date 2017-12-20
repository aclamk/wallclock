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





monitored_thread* xxxx;
int xxxxcount{0};
void empty_signal(int)
{
  printf("alarm\n");
  xxxxcount++;
  xxxx->signal_interrupt();
}




bool probe(Manager& mgr, std::vector<pid_t>& tids)
{
  long ret;
  //probe_thread pt;
  printf("tids.size()=%d\n",tids.size());
  std::vector<monitored_thread> pts;
  pts.resize(tids.size());

  int conn_fd;
  bool res = true;
  for (size_t i=0; i<tids.size(); i++)
  {
    if (res) res = pts[i].seize(tids[i]);
    //for (auto& t : tids)
    //{
    //  if (res) res = pt.seize(t);
    //}
  }
  //if (!pt.seize(target_pid))
  //  return false;
  //Manager mgr;

  std::vector<uint64_t> contexts;
  contexts.resize(tids.size());
  int wstatus;
  //uint64_t context;
  for (size_t i=0; i<tids.size(); i++)
  {
    if (res) res = mgr.trace_thread_new(contexts[i]);
    if (res) pts[i].set_remote_context(contexts[i]);
  }
  //if (!mgr.trace_thread_new(context))
  //  return false;

  //printf("probe 1 context=%lx\n",context);
  //pt.set_remote_context(context);
  sleep(1);

  printf("R2\n");


  for (size_t i=0; i<tids.size(); i++)
  {
    //if (res) res = pts[i].signal_interrupt();
  }
//  if (! pt.signal_interrupt())
//    return false;
  printf("tids.size()=%d\n",tids.size());
  uint64_t time = 0;
  for (int iter=0;iter<1000;iter++)
  {
    res = true;
    if ((iter%100) == 0)
    std::cout << "it " << iter << "time=" << time/(double)1000000/iter << "ms " << xxxxcount << std::endl;

    usleep(5*1000);

    uint64_t start = - now();
    for (size_t i = 0; i<tids.size(); i++)
    {
#if 1
      xxxx = &pts[i];
      if (!pts[i].signal_interrupt())
        printf("cannot interrupt!\n");
      //if (!pts[i].signal_interrupt())
      //    printf("cannot interrupt!\n");
      struct itimerval curr_value;
      struct itimerval new_value;
      struct itimerval old_value;
      getitimer(ITIMER_REAL, &curr_value);
      curr_value.it_interval.tv_sec = 0;
      curr_value.it_interval.tv_usec = 0;
      curr_value.it_value.tv_sec = 3;
      curr_value.it_value.tv_usec = 0;
      setitimer(ITIMER_REAL,
                &curr_value,
                &old_value);
      //char c[100];
      //res = sleep(100);
      //printf("res=%d errno=%d\n",res,errno);

      //pid_t ppp = waitpid(tids[i], &wstatus, 0);
      struct rusage rusage;
      //printf("(%d) tids[i]=%d\n",xxxxcount.load(), tids[i]);
      pid_t ppp = wait4(tids[i], &wstatus, 0, &rusage);
      //printf("ppp=%d\n",ppp);

      if (ppp == tids[i])
      {
        if (WSTOPSIG(wstatus) == SIGTRAP)
        {
          bool b;
          b = pts[i].grab_callback();
          assert(b);
        }
        else
        {
          std::cout << "not SIGTRAP " << WSTOPSIG(wstatus) << std::endl;
        }
        //break;
      }
      else
      {
        std::cout << "!ppp" << std::endl;
        abort();
      }
      //usleep(1*1000);
#endif
    }



    start += now();
   // std::cout << start/(double)1000000 << "ms" << std::endl;
    time += start;
  }

  for (size_t i=0; i<tids.size(); i++)
  {
    mgr.dump_tree(contexts[i]);
  }
  printf("dump end\n");
}












int static_tid = 0;




#define STACK_SIZE (1024 * 1024)
void* locate_library(pid_t pid, const std::string& library_name);

int main(int argc, char** argv)
{
  std::vector<pid_t> tids;

  pid_t v;
  signal(SIGALRM, empty_signal);
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

  printf("ADD=%lx\n", (uint64_t)locate_library(v,"agent.so"));
  Manager mgr;
  init_agent_interface(mgr, v);
  //int pid = atoi(argv[1]);

  //std::cout << "TID=" << tid << std::endl;
  tid = v;
  //tid=pid;
  std::cout << "TID=" << tid << std::endl;
  static_tid = 0;

  probe(mgr, tids);

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
