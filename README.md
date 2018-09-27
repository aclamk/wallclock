Wallclock
=========
Tool to show break-down execution time. Uses wall clock time, to include I/O and mutex waiting time.

1.Targets
---------

Tool is adapted for x86_64-linux-gnu only.

2.Building
----------
Run ./prepare.sh to fetch git submodules.
Run make to build.

3.Usage
-------
3.1.Target preparation
Target binary must be run with LD_PRELOAD=agent.so

3.2.Options

wallclock [-d interval] [-t duration] tid tid tid ....
-d intended interval between sampling in miliseconds
-t duration of sampling

4.Notes
-------
LD_PRELOAD=agent.so is temporary patch, until loading of parasite agent is improved.

5.Debugging
-----------
Injected remote agent works within memory space of other process.
To debug it via gdb one must load "symbol-file" that will supply symbols for agent.
On start, if verbosity is at least 1, wallclock prints:

`Agent loaded to address 0x407d9000`

To obtain symbol-file you must invoke from wallclock build dir:

`make agent.0x407d9000.debug`

And created agent.0x407d9000.debug elf executable will supply necessary symbols.

If initialization procedure fails, --pause option to wallclock instructs agent to halt until ptrace connection is done. 
This means either gdb or strace.
*Beware* agent bootup procedure modifies relocations in code. Breakpoints will work, but do not change any memory that holds relocations.

