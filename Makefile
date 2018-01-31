all: wallclock testprog copy_header callstep.o loader.o agent.so agent.bin agent.elf agent_0x10000000

.phony: libunwind liblzma

libunwind: libunwind/src/.libs/libunwind.a
libunwind/src/.libs/libunwind.a:
	cd libunwind; ./autogen.sh
	make -C libunwind CFLAGS=-fpic
	
LIBUNWIND = libunwind/src/.libs/libunwind.a

liblzma: liblzma/src/liblzma/.libs/liblzma.a
liblzma/src/liblzma/.libs/liblzma.a:
	cd liblzma && ./autogen.sh && ./configure
	make -C liblzma
	
LIBLZMA = liblzma/src/liblzma/.libs/liblzma.a

GCC_VERSION:=$(shell gcc -dumpversion)
GCC_MACHINE:=$(shell gcc -dumpmachine)
	
OBJS = \
	wrapper.o \
	manager.o \
	loader.o \
	callstep.o \
	agent.o \
	largecode.o \
	wallclock.o \
	unix_io.o \
	agent.so

clean:
	echo $(GCC_MACHINE) $(GCC_VERSION)
	rm -f $(OBJS)
	
#-Wl,--oformat -Wl,binary
agent: agent.o Makefile wrapper.o callstep.o unix_io.o libunwind liblzma
	g++ -Wl,--start-group wrapper.o agent.o callstep.o unix_io.o \
	-fPIE -s -Wl,--oformat -Wl,binary -static -pie -T script \
	-nostdlib \
	/usr/lib/x86_64-linux-gnu/libc.a \
	/usr/lib/gcc/x86_64-linux-gnu/6/libstdc++.a \
	/usr/lib/x86_64-linux-gnu/libpthread.a \
	/usr/lib/gcc/x86_64-linux-gnu/6/libgcc_eh.a \
	$(LIBUNWIND) $(LIBLZMA) \
	-Wl,--end-group


agent.so: agent.o wrapper.o callstep.o unix_io.o libunwind liblzma
	g++ -shared -Wl,-export-dynamic -Wl,-soname,agent.so \
    -o agent.so agent.o wrapper.o callstep.o unix_io.o \
    $(LIBUNWIND) $(LIBLZMA) -pthread

#-fuse-ld=gold /usr/lib/x86_64-linux-gnu/Scrt1.o 
SOBJS = /usr/lib/x86_64-linux-gnu/Scrt1.o \
	/usr/lib/x86_64-linux-gnu/crti.o \
	/usr/local/lib/gcc/x86_64-pc-linux-gnu/7.2.0/crtbeginS.o \
	./libunwind/src/.libs/libunwind.a \
	/usr/local/lib/gcc/x86_64-pc-linux-gnu/7.2.0/../../../../lib64/libstdc++.a \
	/usr/lib/x86_64-linux-gnu/libm.a \
	/usr/local/lib/gcc/x86_64-pc-linux-gnu/7.2.0/libgcc.a \
	/usr/local/lib/gcc/x86_64-pc-linux-gnu/7.2.0/libgcc_eh.a \
	/usr/lib/x86_64-linux-gnu/libpthread.a \
	/usr/lib/x86_64-linux-gnu/libc.a \
	/usr/local/lib/gcc/x86_64-pc-linux-gnu/7.2.0/crtendS.o \
	/usr/lib/x86_64-linux-gnu/crtn.o

#-Wl,--oformat -Wl,binary 
# -nostdlib $(SOBJS) -T script1 -fuse-ld=gold -Wl,--debug -Wl,script -Wl,-verbose 
#-nostdlib $(SOBJS) -T script2
#-fuse-ld=gold 

plus = $(shell echo $$(( $(1) + $(2) )) )

agent.bin: agent_0x00000000
	cp $^ $@

agent_bin_%: agent.elf Makefile agent.o wrapper.o callstep.o unix_io.o libunwind liblzma
	g++ -fuse-ld=gold -static -s -Wl,--start-group -Wl,--oformat -Wl,binary \
	-fPIE -fpic -nostdlib $(SOBJS) agent.o wrapper.o callstep.o unix_io.o \
    $(LIBUNWIND) $(LIBLZMA) -pthread -Wl,-Map=map.$* -Wl,--end-group \
    -Ttext=$(call plus, $*, 0x1000) -o $@ 

.PRECIOUS: agent_%
	    
agent_%: agent_bin_% agent.elf copy_header    
	tail -c +$(call plus, $*, 1) $< > $@
	./copy_header agent.elf $$(sed -n '/ _start$$/ s/_start// p' map.$*) $@
	
header: agent.elf map.0x00000000 copy_header
	./copy_header agent.elf $$(sed -n '/ _start$$/ s/_start// p' map.0x00000000) $@

header1: agent1.elf map.0x00000000 copy_header
	./copy_header agent.elf $$(sed -n '/ _start$$/ s/_start// p' map.0x00000000) $@


rel.bin: find_relocs agent_0x00000000 agent_0x12345000
	./find_relocs agent_0x00000000 agent_0x12345000 0x12345000 $@

rel_%: find_relocs agent_0x00000000 agent_% rel.bin
	./find_relocs agent_0x00000000 agent_$* $* $@
	diff $@ rel.bin


header.o: header
	objcopy --rename-section .data=.header -I binary header -O elf64-x86-64 -B i386 header.o

rel.bin.o: rel.bin
	objcopy --rename-section .data=.rel.bin -I binary rel.bin -O elf64-x86-64 -B i386 rel.bin.o
	
agent.bin.o: agent.bin
	objcopy --rename-section .data=.agent.bin -I binary agent.bin -O elf64-x86-64 -B i386 agent.bin.o

#-Wl,--oformat -Wl,binary 
pagent.rel: syscall.o init_agent.o  call_start.o rel.bin.o header.o agent.bin.o
	g++ -fuse-ld=gold -Wl,--oformat -Wl,binary -Wl,-Map=map.pagent.rel \
	-static -s -nostdlib -fPIE -fpic \
	-Wl,--start-group $^ -Wl,--end-group -o $@ -T script-loader

rel_check: rel_0x00112000 rel_0x13579000 rel_0x2648a000 rel_0x18375000
	    	
agent.elf: Makefile agent.o wrapper.o callstep.o unix_io.o libunwind liblzma
	g++ -fuse-ld=gold -Ttext=0x10001000 -Wl,--start-group -static \
	-fPIE -fpic -nostdlib $(SOBJS) \
    -o agent.elf agent.o wrapper.o callstep.o unix_io.o \
    $(LIBUNWIND) $(LIBLZMA) -pthread -Wl,--end-group

agent1.elf: Makefile agent.o wrapper.o callstep.o unix_io.o libunwind liblzma
	g++ -fuse-ld=gold -Ttext=0x12341000 -Wl,--start-group -static \
	-fPIE -fpic -nostdlib $(SOBJS) \
    -o agent.elf agent.o wrapper.o callstep.o unix_io.o \
    $(LIBUNWIND) $(LIBLZMA) -pthread -Wl,--end-group
    

    
#WC_OPTS = -fno-omit-frame-pointer 
WC_OPTS = -O0 -g -Ielfio

manager.o: manager.cpp
	g++ -c $< -o $@ $(WC_OPTS) 
	
loader.o: loader.cpp
	g++ -c $< -o $@ $(WC_OPTS) -fPIC

unix_io.o: unix_io.cpp
	g++ -c $< -o $@ $(WC_OPTS) -fPIC

tls.o: tls.cpp
	g++ -c $< -o $@ $(WC_OPTS) -fPIC

callstep.o: callstep.cpp
	g++ -c $< -o $@ $(WC_OPTS) -fPIC

agent.o: agent.cpp
	g++ -c $< -o $@ $(WC_OPTS) -fPIC

largecode.o: largecode.cpp
	g++ -c $< -o $@ $(WC_OPTS)


wallclock.o: wallclock.cpp
	g++ -c $< -o $@ $(WC_OPTS) -fPIC

call_start.o: call_start.asm
	as -c $< -o $@

init_agent.o: init_agent.c
	gcc -g -O3 -c $< -o $@ -fPIC

wrapper.o: injected/wrapper.asm
	as -c $< -o $@
	
syscall.o: syscall.asm
	as -c $< -o $@
	

test_agent_0x10000000: test_agent_0x10000000.cpp agent_0x10000000 call_start.o
	g++ -static $< call_start.o -o $@

test_agent_relocated: test_agent_relocated.cpp agent_0x00000000 call_start.o rel.bin.o
	g++ -static $< call_start.o rel.bin.o -o $@

test_agent_anyplace: test_agent_anyplace.cpp pagent.rel
	g++ -static $< -o $@
	
	
	
wallclock: wallclock.o callstep.o agent.so manager.o loader.o unix_io.o call_start.o
	g++ -o $@ $^ -lpthread -lunwind 

testprog: testprog.cpp largecode.o
	g++ $^ -o $@ -lpthread
	
copy_header: copy_header.cpp
	g++ -o $@ $^ $(WC_OPTS) 
	
find_relocs: find_relocs.cpp
	g++ -o $@ $^ $(WC_OPTS) 
	
init_agent: init_agent.o syscall.o
	g++ -o $@ $^