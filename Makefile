all: wallclock agent.so pagent.rel
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
$(OBJS_WALLCLOCK) $(OBJS_BOOTUP) $(OBJS_AGENT): $(OBJ_DIRS)
$(OBJ_DIRS):
	mkdir -p $@


clean:
	echo $(GCC_MACHINE) $(GCC_VERSION)
	rm -f $(OBJS)
	
#-Wl,--oformat -Wl,binary
agent: $(OBJS_AGENT) libunwind liblzma Makefile
	g++ -Wl,--start-group $(OBJS_AGENT) \
	-fPIE -s -Wl,--oformat -Wl,binary -static -pie -T script \
	-nostdlib \
	/usr/lib/x86_64-linux-gnu/libc.a \
	/usr/lib/gcc/x86_64-linux-gnu/6/libstdc++.a \
	/usr/lib/x86_64-linux-gnu/libpthread.a \
	/usr/lib/gcc/x86_64-linux-gnu/6/libgcc_eh.a \
	$(LIBUNWIND) $(LIBLZMA) \
	-Wl,--end-group


agent.so: $(OBJS_AGENT) libunwind liblzma Makefile
	g++ -shared -Wl,-export-dynamic -Wl,-soname,agent.so \
    -o agent.so $(OBJS_AGENT) $(LIBUNWIND) $(LIBLZMA) -pthread

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

agent_bin_% map.%: agent.%.elf Makefile $(OBJS_AGENT) libunwind liblzma
	g++ -fuse-ld=gold -static -s -Wl,--start-group -Wl,--oformat -Wl,binary \
	-fPIE -fpic -nostdlib $(SOBJS) $(OBJS_AGENT) \
    $(LIBUNWIND) $(LIBLZMA) -pthread -Wl,-Map=map.$* -Wl,--end-group \
    -Ttext=$(call plus, $*, 0x1000) -o agent_bin_$*

.PRECIOUS: agent_% 

header_%: agent.%.elf map.% copy_header
	./copy_header agent.$*.elf $$(sed -n '/ _start$$/ s/_start// p' map.$*) header_$*
	
agent_nh_%: agent_bin_%
	tail -c +$(call plus, $*, 0x1001) agent_bin_$* > agent_nh_$*

agent_wh_%: agent_bin_% agent.%.elf map.% copy_header    
	tail -c +$(call plus, $*, 1) agent_bin_$* > agent_wh_$*
	./copy_header agent.$*.elf $$(sed -n '/ _start$$/ s/_start// p' map.$*) agent_wh_$*



header: header_0x00000000
	cp $< $@ 

agent_nh.bin: agent_nh_0x00000000
	cp $< $@

header1: agent1.elf map.0x00000000 copy_header
	./copy_header agent.elf $$(sed -n '/ _start$$/ s/_start// p' map.0x00000000) $@


rel.bin: find_relocs agent_wh_0x00000000 agent_wh_0x12345000
	./find_relocs agent_wh_0x00000000 agent_wh_0x12345000 0x12345000 $@

rel_%: find_relocs agent_wh_0x00000000 agent_wh_% rel.bin
	./find_relocs agent_wh_0x00000000 agent_wh_$* $* $@
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

rel_check: rel_0x00112000 rel_0x13579000 rel_0x2648a000 rel_0x18375000
	 
	    	
agent.elf: Makefile $(OBJS_AGENT) libunwind liblzma
	g++ -fuse-ld=gold -Ttext=0x10001000 -Wl,--start-group -static \
	-fPIE -fpic -nostdlib $(SOBJS) \
    -o agent.elf $(OBJS_AGENT) \
    $(LIBUNWIND) $(LIBLZMA) -pthread -Wl,--end-group

agent.%.elf: $(OBJS_AGENT) libunwind liblzma Makefile
	g++ -fuse-ld=gold -Ttext=$(call plus, $*, 0x1000) -Wl,--start-group -static \
	-fPIE -fpic -nostdlib -Wl,-Map=agent.$*.map $(SOBJS) \
    -o agent.$*.elf $(OBJS_AGENT) \
    $(LIBUNWIND) $(LIBLZMA) -pthread -Wl,--end-group
    

#WC_OPTS = -fno-omit-frame-pointer 
WC_OPTS = -O0 -g -Ielfio
DEBUG = -O0 -g

$(OBJS_AGENT_CPP): obj/agent/%.o: src/%.cpp
	g++ -c $< -o $@ -fPIC -Ielfio $(DEBUG)
$(OBJS_AGENT_ASM): obj/agent/%.o: src/%.asm
	as -c $< -o $@

$(OBJS_BOOTUP_C): obj/bootup/%.o: src/%.c
	gcc -c $< -o $@ -fPIC $(DEBUG)
$(OBJS_BOOTUP_ASM): obj/bootup/%.o: src/%.asm
	as -c $< -o $@

$(OBJS_WALLCLOCK): %.o: src/%.cpp
	g++ -c $< -o $@ $(DEBUG)

$(OBJS_WALLCLOCK) $(OBJS_BOOTUP) $(OBJS_AGENT): Makefile


test_agent_0x10000000: test_agent_0x10000000.cpp agent_wh_0x10000000 call_start.o
	g++ -static $< call_start.o -o $@

test_agent_relocated: test_agent_relocated.cpp agent_wh_0x00000000 call_start.o rel.bin.o
	g++ -static $< call_start.o rel.bin.o -o $@

test_agent_anyplace: test_agent_anyplace.cpp pagent.rel
	g++ -static $< -o $@
	
	
	
wallclock: wallclock.o callstep.o manager.o loader.o unix_io.o call_start.o agent.so 
	g++ -o $@ $^ -lpthread -lunwind 

testprog: testprog.cpp largecode.o
	g++ $^ -o $@ -lpthread
	
copy_header: src/copy_header.cpp
	g++ -o $@ $^ $(WC_OPTS) 
	
find_relocs: src/find_relocs.cpp
	g++ -o $@ $^ $(WC_OPTS) 
	
init_agent: init_agent.o syscall.o
	g++ -o $@ $^