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

extern "C"
void _remote_return(uint64_t a, uint64_t b, uint64_t c);

std::vector<pid_t> tids;
std::atomic<bool> stop_probing{false};


bool probe(Manager& mgr)
{
  bool res = true;
  long ret;
  for (size_t i=0; i<tids.size(); i++)
  {
    if (res) res = mgr.trace_attach(tids[i]);
  }
  uint64_t time = 0;

  for (int iter = 0; iter < 10000 && (stop_probing.load() == false); iter++)
  {
    uint64_t start = -now();
    res = true;
    if ((iter%100) == 0)
      std::cout << "it " << iter << "time=" << time/(double)1000000/iter << "ms " << std::endl;
    res = mgr.probe();
    assert(res);
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




void stop_sampling(int signum, siginfo_t *info, void *ctx)
{
  stop_probing.store(true);
}



int main(int argc, char** argv)
{
  pid_t v;

  if (argc < 2)
  {
    std::cerr << "no target" << std::endl;
    exit(-1);
  }
  else
  {
    for (int i = 1; i < argc ; i++)
    {
      v = atoi(argv[i]);
      tids.push_back(v);
    }
  }

  Manager mgr;
  init_agent_interface(mgr, v);

  struct sigaction stop_sampling_sig;

  sigemptyset(&stop_sampling_sig.sa_mask);
  sigaddset(&stop_sampling_sig.sa_mask, SIGINT);
  stop_sampling_sig.sa_flags = SA_SIGINFO | SA_RESTART;
  stop_sampling_sig.sa_sigaction = stop_sampling;
//  int sigaction(int signum, const struct sigaction *act,
//                       struct sigaction *oldact);
  sigaction(SIGINT, &stop_sampling_sig, nullptr);
  //signal(SIG_INT,stop_sampling);

  probe(mgr);
}
