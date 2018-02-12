all: dirs wallclock agent.so pagent.rel
tests: testprog test_agent_0x10000000 test_agent_anyplace test_agent_relocated

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


OBJS_AGENT_ASM = obj/agent/wrapper.o 
OBJS_AGENT_CPP = obj/agent/agent.o \
                 obj/agent/callstep.o \
                 obj/agent/unix_io.o
OBJS_AGENT = $(OBJS_AGENT_ASM) $(OBJS_AGENT_CPP)

OBJS_BOOTUP_ASM = obj/bootup/bootup_agent.o \
                  obj/bootup/syscall.o \
                  obj/bootup/call_start.o 
OBJS_BOOTUP_C =   obj/bootup/init_agent.o
OBJS_BOOTUP = $(OBJS_BOOTUP_ASM) $(OBJS_BOOTUP_C)

OBJS_WALLCLOCK = obj/wallclock/manager.o \
                 obj/wallclock/loader.o \
                 obj/wallclock/wallclock.o \
                 obj/wallclock/unix_io.o

OBJS = $(OBJS_WALLCLOCK) $(OBJS_BOOTUP) $(OBJS_AGENT)
OBJ_DIRS = $(sort $(dir $(OBJS_WALLCLOCK) $(OBJS_BOOTUP) $(OBJS_AGENT)))
#$(OBJS_WALLCLOCK) $(OBJS_BOOTUP) $(OBJS_AGENT): $(OBJ_DIRS)

DIRS = bin res $(OBJ_DIRS)
dirs: $(DIRS)
$(DIRS):
	mkdir -p $@ 

clean:
	echo $(GCC_MACHINE) $(GCC_VERSION)
	rm -f $(OBJS)

testprog: bin/testprog
wallclock: bin/wallclock

agent.so: $(OBJS_AGENT) libunwind liblzma Makefile
	g++ -shared -Wl,-export-dynamic -Wl,-soname,agent.so \
    -o agent.so $(OBJS_AGENT) $(LIBUNWIND) $(LIBLZMA) -pthread

SOBJS_SRC = \
	Scrt1.o \
	crti.o \
	crtn.o \
	libm.a \
	libpthread.a \
	libc.a \
	crtbeginS.o \
	libgcc.a \
	libgcc_eh.a \
	crtendS.o \
	libstdc++.a

SOBJS = $(foreach file,$(SOBJS_SRC),$(shell gcc -print-file-name=$(file)))

plus = $(shell echo $$(( $(1) + $(2) )) )

agent.bin: agent_0x00000000
	cp $^ $@

res/agent_bin_% res/map.%: res/agent.%.elf $(OBJS_AGENT) libunwind liblzma Makefile 
	g++ -fuse-ld=gold -static -s -Wl,--start-group -Wl,--oformat -Wl,binary \
	-fPIE -fpic -nostdlib $(SOBJS) $(OBJS_AGENT) \
    $(LIBUNWIND) $(LIBLZMA) -pthread -Wl,-Map=res/map.$* -Wl,--end-group \
    -Ttext=$(call plus, $*, 0x1000) -o res/agent_bin_$*

.PRECIOUS: agent_% 

res/header_%: res/agent.%.elf res/map.% copy_header
	./copy_header res/agent.$*.elf $$(sed -n '/ _start$$/ s/_start// p' res/map.$*) res/header_$*
	
res/agent_nh_%: res/agent_bin_%
	tail -c +$(call plus, $*, 0x1001) res/agent_bin_$* > res/agent_nh_$*

res/agent_wh_%: res/agent_bin_% res/agent.%.elf res/map.% copy_header    
	tail -c +$(call plus, $*, 1) res/agent_bin_$* > res/agent_wh_$*
	./copy_header res/agent.$*.elf $$(sed -n '/ _start$$/ s/_start// p' res/map.$*) res/agent_wh_$*



header: res/header_0x00000000
	cp $< $@ 

agent_nh.bin: res/agent_nh_0x00000000
	cp $< $@


rel.bin: find_relocs res/agent_wh_0x00000000 res/agent_wh_0x12345000
	./find_relocs res/agent_wh_0x00000000 res/agent_wh_0x12345000 0x12345000 $@

rel_%: find_relocs res/agent_wh_0x00000000 res/agent_wh_% rel.bin
	./find_relocs res/agent_wh_0x00000000 res/agent_wh_$* $* $@
	diff $@ rel.bin


header.o: header
	objcopy --rename-section .data=.header -I binary header -O elf64-x86-64 -B i386 header.o

rel.bin.o: rel.bin
	objcopy --rename-section .data=.rel.bin -I binary rel.bin -O elf64-x86-64 -B i386 rel.bin.o
	
agent_nh.bin.o: agent_nh.bin
	objcopy --rename-section .data=.agent.bin -I binary agent_nh.bin -O elf64-x86-64 -B i386 agent_nh.bin.o

POBJS_AGENT = $(OBJS_BOOTUP) rel.bin.o header.o agent_nh.bin.o
 
pagent.rel: $(POBJS_AGENT) Makefile script-loader 
	g++ -fuse-ld=gold -Wl,--oformat -Wl,binary -Wl,-Map=map.pagent.rel \
	-static -s -nostdlib -fPIE -fpic \
	-Wl,--start-group $(POBJS_AGENT) -Wl,--end-group -o $@ -T script-loader

rel_check: res/rel_0x00112000 res/rel_0x13579000 res/rel_0x2648a000 res/rel_0x18375000
	 
	    	
bin/agent.elf: $(OBJS_AGENT) libunwind liblzma Makefile 
	g++ -fuse-ld=gold -Ttext=0x10001000 -Wl,--start-group -static \
	-fPIE -fpic -nostdlib $(SOBJS) \
    -o bin/agent.elf $(OBJS_AGENT) \
    $(LIBUNWIND) $(LIBLZMA) -pthread -Wl,--end-group

res/agent.%.elf: $(OBJS_AGENT) libunwind liblzma Makefile
	g++ -fuse-ld=gold -Ttext=$(call plus, $*, 0x1000) -fPIE -fpic -nostdlib -static \
	-Wl,--start-group $(SOBJS) $(OBJS_AGENT) $(LIBUNWIND) $(LIBLZMA) -Wl,--end-group \
    -Wl,-Map=res/agent.$*.map -o res/agent.$*.elf 

DEBUG = -O0 -g

$(OBJS_AGENT_CPP): obj/agent/%.o: src/%.cpp
	g++ -c $< -o $@ -fPIC -Ielfio $(DEBUG)
$(OBJS_AGENT_ASM): obj/agent/%.o: src/%.asm
	as -c $< -o $@

$(OBJS_BOOTUP_C): obj/bootup/%.o: src/%.c
	gcc -c $< -o $@ -fPIC $(DEBUG)
$(OBJS_BOOTUP_ASM): obj/bootup/%.o: src/%.asm
	as -c $< -o $@

$(OBJS_WALLCLOCK): obj/wallclock/%.o: src/%.cpp
	g++ -c $< -o $@ $(DEBUG)

obj/%.o: src/%.cpp
	g++ -c $< -o $@ $(DEBUG)

$(OBJS_WALLCLOCK) $(OBJS_BOOTUP) $(OBJS_AGENT): Makefile

test_agent_0x10000000: bin/test_agent_0x10000000
test_agent_relocated: bin/test_agent_relocated
test_agent_anyplace: bin/test_agent_anyplace

bin/test_agent_0x10000000: src/test_agent_0x10000000.cpp res/agent_wh_0x10000000 obj/bootup/call_start.o
	g++ -static $< obj/bootup/call_start.o -o $@

bin/test_agent_relocated: src/test_agent_relocated.cpp res/agent_wh_0x00000000 obj/bootup/call_start.o rel.bin.o
	g++ -static $< obj/bootup/call_start.o rel.bin.o -o $@

bin/test_agent_anyplace: src/test_agent_anyplace.cpp pagent.rel
	g++ -static $< -o $@
	
	
bin/wallclock: $(OBJS_WALLCLOCK) agent.so bin
	g++ -o $@ $(OBJS_WALLCLOCK) agent.so -lpthread -lunwind 

bin/testprog: src/testprog.cpp obj/largecode.o bin
	g++ src/testprog.cpp obj/largecode.o -o $@ -lpthread
	
copy_header: src/copy_header.cpp
	g++ -o $@ $^ -Ielfio $(DEBUG)
	
find_relocs: src/find_relocs.cpp
	g++ -o $@ $^ $(DEBUG)
	
init_agent: init_agent.o syscall.o
	g++ -o $@ $^
