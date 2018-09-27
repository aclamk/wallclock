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

#include <set>
uint64_t now()
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec*1000000000 + t.tv_nsec;
}

std::atomic<bool> stop_probing{false};
int delay = 10;
int sampling_time = 10;
uint64_t current_time;
int verbose_level = 0;

std::set<pid_t> list_threads_in_group(pid_t pid);

bool probe(Manager& mgr, const std::set<pid_t>& tids)
{
  bool res = true;
  long ret;
  if (verbose_level >= 4)
    std::cerr << "Registering probes" << std::endl;
  for (auto i = tids.begin(); i != tids.end(); i++)
  {
    res = mgr.trace_attach(*i);
    if (!res)
    {
      if (verbose_level >= 1)
        std::cerr << "Cannot probe thread " << *i << std::endl;
    } else {
      if (verbose_level >= 4)
        std::cerr << "Probing thread " << *i << std::endl;
    }
  }
  if (verbose_level >= 4)
    std::cerr << "Registering probes done" << std::endl;

  uint64_t begin = now();
  uint64_t last_notified = begin - 1000*1000*1000;

  int iter = 0;
  do
  {
    uint64_t tnow_b = now();
    if (tnow_b - last_notified >= 1000*1000*1000)
    {
      last_notified = tnow_b;
      if (verbose_level >= 1 && verbose_level < 5) std::cerr << "samples: " << iter << "\r" << std::flush;
      if (verbose_level >=5) std::cerr << "samples: " << iter << std::endl;
    }
    res = mgr.probe();
    assert(res);
    current_time = now();

    if ((current_time - tnow_b) / 1000 < delay * 1000) //in usec
      usleep( delay * 1000 - (current_time - tnow_b) / 1000);
    iter++;
  }
  while(stop_probing.load() == false && (current_time - begin) < (uint64_t)sampling_time * 1000 * 1000 * 1000);
  return res;
}

void stop_sampling(int signum, siginfo_t *info, void *ctx)
{
  stop_probing.store(true);
}

void print_help() {
  static const char help[] =
"Usage: wallclock [OPTION]... [TID]...\n"
"Query remote program for split of execution time.\n"
"Factors in periods of waiting for cpu, both synchronization and deschedule.\n"
"\n"
" -p pid                  Sample all threads in process <pid>\n"
" -o FILE                 Write output to FILE\n"
" -t TIME(s)              TIME of sampling, in seconds. Default 10\n"
" -d TIME(ms)             TIME interval between samples, in miliseconds. Default 10\n"
" -s LIMIT                Suppress branches consuming below LIMIT. Default 0\n"
" -so                     Attempt to use libagent.so. Default is to injected code.\n"
" --pause                 Pause agent until ptrace/gdb connected (debug).\n"
" -l, --no-unload         Do not unload agent on exit.\n"
" -v -v -v                Increase verbosity.\n"
" -h, --help              Help\n";
  std::cout << help << std::endl;
}

bool unmap_memory(pid_t remote_leader,
                  const std::vector<std::pair<uint64_t, uint64_t>>& regions);

int main(int argc, char** argv)
{
  std::ofstream outfile;
  std::ostream* output = &std::cout;
  double suppress = 0;
  bool load_so = false;
  bool wait_for_ptrace = false;
  bool unload_on_exit = true;
  int i = 1;
  std::set<pid_t> tids;
  std::set<pid_t> group;
  char* endp;
  argc--; argv++;
  std::set<std::string> oneparam_args={"-t", "-d", "-o", "-s", "-p"};
  while (argc >= 1)
  {
    std::string arg = *argv;
    argc--; argv++;

    if (oneparam_args.count(arg) > 0) {
      //one param argument
      if (argc < 1) {
        std::cerr << "Option " << arg << " requires parameter" << std::endl;
        exit(-1);
      }
      std::string value = *argv;
      if (arg == "-t") {
        sampling_time = strtol(*argv, &endp, 0);
        if (*endp != '\0') {
          std::cerr << "Option -t cannot accept `" << value << "'" << std::endl;
          exit(-1);
        }
      }
      if (arg == "-d") {
        delay = strtol(*argv, &endp, 0);
        if (*endp != '\0') {
          std::cerr << "Option -d cannot accept `" << value << "'" << std::endl;
          exit(-1);
        }
      }
      if (arg == "-o") {
        outfile = std::ofstream(value.c_str(),std::ofstream::binary|std::ofstream::trunc);
        if (!outfile.good()) {
          std::cerr << "Cannot open '" << value << "'" << std::endl;
          exit(-1);
        } else {
          output = &outfile;
        }
      }
      if (arg == "-s") {
        suppress = strtod(*argv, &endp) / 100;
        if (*endp != '\0') {
          std::cerr << "Invalid float '" << value << "'." << std::endl;
          exit(-1);
        }
      }
      if (arg == "-p") {
        pid_t wholepid = strtol(*argv, &endp, 0);
        if (*endp != '\0') {
          std::cerr << "Option -p cannot accept `" << value << "'." << std::endl;
          exit(-1);
        }
        if (tids.size() > 0) {
          std::cerr << "Cannot select whole process, when threads already selected." << std::endl;
          exit(-1);
        }
        tids = list_threads_in_group(wholepid);
        group = tids;
        if (tids.size() == 0) {
          std::cerr << "Cannot access process " << wholepid << "." << std::endl;
          exit(-1);
        }
      }
      argc--; argv++;
      continue;
    }

    if (arg == "-so") {
      load_so = true;
      continue;
    }
    if (arg == "--pause") {
      wait_for_ptrace = true;
      continue;
    }
    if (arg == "-v") {
      verbose_level++;
      continue;
    }
    if (arg == "-l" || arg == "--no-unload") {
      unload_on_exit = false;
      continue;
    }

    if (arg == "-h" || arg == "--help") {
      print_help();
      exit(-1);
    }
    //non-prefixed parameter = pid
    pid_t tid;
    tid = strtol(arg.c_str(), &endp, 0);
    if (*endp != '\0')
    {
      std::cerr << "Cannot accept `" << arg << "' as thread-id" << std::endl;
      exit(-1);
    }
    if (group.size() != 0) {
      if (group.count(tid) == 0) {
        std::cerr << "Thread `" << tid << "' not in the same process as previously selected." << std::endl;
        exit(-1);
      }
    } else {
      group = list_threads_in_group(tid);
    }
    tids.insert(tid);
  }

  if (tids.size() == 0)
  {
    std::cerr << "No thread-ids provided" << std::endl;
    exit(-1);
  }

  Manager mgr;
  struct sigaction stop_sampling_sig;

  if (!init_agent_interface(mgr, *tids.begin(), load_so, wait_for_ptrace)) {
    std::cerr << "Failed to initialize remote agent." << std::endl;
    return false;
  }

  sigemptyset(&stop_sampling_sig.sa_mask);
  sigaddset(&stop_sampling_sig.sa_mask, SIGINT);
  stop_sampling_sig.sa_flags = SA_SIGINFO | SA_RESTART;
  stop_sampling_sig.sa_sigaction = stop_sampling;
  sigaction(SIGINT, &stop_sampling_sig, nullptr);

  mgr.read_symbols();
  probe(mgr, tids);
  for (auto i = tids.begin(); i != tids.end(); i++) {
    mgr.dump_tree(*output, *i, suppress);
  }
  if (outfile.is_open()) {
    outfile.close();
  }
  if (unload_on_exit) {
    std::vector<std::pair<uint64_t, uint64_t>> regions;
    mgr.get_memory(regions);
    mgr.terminate();
    unmap_memory(*tids.begin(), regions);
  }
}
