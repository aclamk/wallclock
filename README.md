Wallclock
Tool to show break-down execution time. Uses wall clock time, to include I/O and mutex waiting time.

1. Targets
Tool is adapted for x86_64-linux-gnu only.

2. Building
Run ./prepare.sh to fetch git submodules.
Run make to build.

3. Usage

3.1 Target preparation
Target binary must be run with LD_PRELOAD=agent.so

3.2 Options

wallclock [-d interval] [-t duration] tid tid tid ....
-d intended interval between sampling in miliseconds
-t duration of sampling

4. Notes

LD_PRELOAD=agent.so is temporary patch, until loading of parasite agent is improved.