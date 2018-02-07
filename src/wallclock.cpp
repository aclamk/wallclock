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

#include <getopt.h>
#include <string.h>
#include <fstream>
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
int delay = 10;
int sampling_time = 10;
uint64_t current_time;

bool probe(Manager& mgr)
{
  bool res = true;
  long ret;
  for (size_t i=0; i<tids.size(); i++)
  {
    res = mgr.trace_attach(tids[i]);
    if (!res)
    {
      std::cerr << "Cannot probe thread " << tids[i] << std::endl;
    }
  }
  uint64_t begin = now();
  uint64_t last_notified = begin - 1000*1000*1000;

  //for (int iter = 0; iter < 10000 && (stop_probing.load() == false); iter++)
  int iter = 0;
  do
  {
    uint64_t tnow_b = now();
    if (tnow_b - last_notified >= 1000*1000*1000)
    {
      last_notified = tnow_b;
      std::cout << "samples: " << iter << "\r" << std::flush;
    }
//    if ((iter%100) == 0)
//      std::cout << "it " << iter << "time=" << time/(double)1000000/iter << "ms " << std::endl;

    res = mgr.probe();
    assert(res);
    current_time = now();

//    if (start > 10*1000*1000) {
//      std::cout << "iteration took " << start << " ns" << std::endl;
//    }
    if ((current_time - tnow_b) / 1000 < delay * 1000) //in usec
      usleep( delay * 1000 - (current_time - tnow_b) / 1000);
//    time += start;
    iter++;
  }
  while(stop_probing.load() == false && (current_time - begin) < (uint64_t)sampling_time * 1000 * 1000 * 1000);
}




void stop_sampling(int signum, siginfo_t *info, void *ctx)
{
  stop_probing.store(true);
}



int main(int argc, char** argv)
{
  std::ofstream outfile;
  std::ostream* output = &std::cout;
  double suppress = 0;
  int i = 1;
  argc--; argv++;

  while (argc >= 1)
  {
    if(strcmp(*argv, "-t") == 0)
    {
      if (argc >= 2)
      {
        argc--; argv++;
        char* endptr;
        sampling_time = strtol(*argv, &endptr, 10);
        if (*endptr!= '\0')
        {
          std::cerr << "Option -t cannot accept `" << *argv << "'" << std::endl;
          exit(-1);
        }
        argc--; argv++;
        continue;
      }
      else
      {
        std::cerr << "Option -t requires parameter" << std::endl;
        exit(-1);
      }
    }
    if(strcmp(*argv, "-d") == 0)
    {
      if (argc >= 2)
      {
        argc--; argv++;
        char* endptr;
        delay = strtol(*argv, &endptr, 10);
        if (*endptr!= '\0')
        {
          std::cerr << "Option -d cannot accept `" << *argv << "'" << std::endl;
          exit(-1);
        }
        argc--; argv++;
        continue;
      }
      else
      {
        std::cerr << "Option -d requires parameter" << std::endl;
        exit(-1);
      }
    }

    if(strcmp(*argv, "-o") == 0)
    {
      if (argc >= 2)
      {
        argc--; argv++;
        outfile = std::ofstream(*argv,std::ofstream::binary|std::ofstream::trunc);
        if (!outfile.good()) {
          std::cerr << "Cannot open '" << *argv << "'" << std::endl;
          exit(-1);
        } else {
          output = &outfile;
        }
        argc--; argv++;
        continue;
      }
      else
      {
        std::cerr << "Option -o requires parameter" << std::endl;
        exit(-1);
      }
    }

    if(strcmp(*argv, "-s") == 0)
    {
      if (argc >= 2)
      {
        argc--; argv++;
        char* endp;
        suppress = strtod(*argv, &endp);
        if (*endp != '\0') {
          std::cerr << "Invalid float '" << *argv << "'" << std::endl;
          exit(-1);
        }
        suppress = suppress / 100;
        argc--; argv++;
        continue;
      }
      else
      {
        std::cerr << "Option -s requires parameter" << std::endl;
        exit(-1);
      }
    }

    //non-prefixed parameter = pid
    char* endptr;
    pid_t tid;
    tid = strtol(*argv, &endptr, 10);
    if (*endptr != '\0')
    {
      std::cerr << "Cannot accept `" << *argv << "' as thread-id" << std::endl;
      exit(-1);
    }
    tids.push_back(tid);
    argc--; argv++;
  }

  if (tids.size() == 0)
  {
    std::cerr << "No thread-ids provided" << std::endl;
    exit(-1);
  }

  Manager mgr;
  init_agent_interface(mgr, tids[0], false);

  struct sigaction stop_sampling_sig;

  sigemptyset(&stop_sampling_sig.sa_mask);
  sigaddset(&stop_sampling_sig.sa_mask, SIGINT);
  stop_sampling_sig.sa_flags = SA_SIGINFO | SA_RESTART;
  stop_sampling_sig.sa_sigaction = stop_sampling;
  sigaction(SIGINT, &stop_sampling_sig, nullptr);

  mgr.read_symbols();
  probe(mgr);
  for (size_t i=0; i<tids.size(); i++) {
    mgr.dump_tree(*output, tids[i], suppress);
  }

  if (outfile.is_open()) {
    outfile.close();
  }

}
