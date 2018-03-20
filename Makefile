all: dirs wallclock libagent.so pagent.rel
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
	make -C liblzma CFLAGS=-fpic
	
LIBLZMA = liblzma/src/liblzma/.libs/liblzma.a

GCC_VERSION:=$(shell gcc -dumpversion)
GCC_MACHINE:=$(shell gcc -dumpmachine)


OBJS_AGENT_ASM = obj/agent/wrapper.o 
OBJS_AGENT_CPP = obj/agent/agent.o \
                 obj/agent/callstep.o \
                 obj/agent/unix_io.o
OBJS_AGENT = $(OBJS_AGENT_ASM) $(OBJS_AGENT_CPP)

OBJS_AGENT_EXTRA = obj/agent/brk.o

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
PROGS = copy_header find_relocs header_size bin/testprog bin/wallclock
RES = res/relocations res/relocations.o res/header res/headers.o

DIRS = bin res $(OBJ_DIRS)
dirs: $(DIRS)
$(DIRS):
	mkdir -p $@ 

clean:
	echo $(GCC_MACHINE) $(GCC_VERSION)
	rm -f $(OBJS) $(PROGS) $(RES) res/*

testprog: bin/testprog
wallclock: bin/wallclock

libagent.so: $(OBJS_AGENT) libunwind liblzma Makefile
	g++ -shared -Wl,-export-dynamic -Wl,-soname,libagent.so \
    -o libagent.so $(OBJS_AGENT) $(LIBUNWIND) $(LIBLZMA) -pthread

SOBJS_SRC = \
	Scrt1.o \
	crti.o \
	crtn.o \
	libm.a \
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

res/agent_bin_% res/agent_bin_%.map: res/agent.%.elf $(OBJS_AGENT_EXTRA) $(OBJS_AGENT) libunwind liblzma Makefile
	g++ -fuse-ld=gold -static -s -Wl,--start-group -Wl,--oformat -Wl,binary \
	-fPIE -fpic -Wl,--build-id=none -nostdlib $(OBJS_AGENT_EXTRA) $(OBJS_AGENT) $(SOBJS) \
    $(LIBUNWIND) $(LIBLZMA) -pthread -Wl,-Map=res/agent_bin_$*.map -Wl,--end-group \
    -Wl,--allow-multiple-definition -Ttext=$* -o res/agent_bin_$*

.PRECIOUS: agent_% 

res/header_%: res/agent.%.elf res/agent_bin_%.map copy_header
	./copy_header res/agent.$*.elf $$(sed -n '/ _start$$/ s/_start// p' res/agent_bin_$*.map) res/header_$*

#res/agent_nh_%: res/agent_bin_%
#	tail -c +$(call plus, $*, 0x1001) res/agent_bin_$* > res/agent_nh_$*

#res/agent_wh_%: res/agent_bin_% res/agent.%.elf res/agent.%.elf.map copy_header    
#	tail -c +$(call plus, $*, 1) res/agent_bin_$* > res/agent_wh_$*
#	./copy_header res/agent.$*.elf $$(sed -n '/ _start$$/ s/_start// p' res/agent.$*.elf.map) res/agent_wh_$*



#res/header: res/header_0x00000000
#	cp $< $@ 

#res/agent_nh.bin: res/agent_nh_0x00000000
#	cp $< $@


#res/relocations: find_relocs res/agent_wh_0x00000000 res/agent_wh_0x12345000
#	./find_relocs res/agent_wh_0x00000000 res/agent_wh_0x12345000 0x12345000 $@

#res/strip.agent.%.elf: res/agent.%.elf 
#	strip $^ -o $@

res/relocations: find_relocs res/agent.0x00000000.wh res/agent.0x12345000.wh
	./find_relocs res/agent.0x00000000.wh res/agent.0x12345000.wh 0x12345000 $@

rel_%: find_relocs res/agent_wh_0x00000000 res/agent_wh_% res/relocations
	./find_relocs res/agent_wh_0x00000000 res/agent_wh_$* $* $@
	diff $@ res/relocations


res/header.o: res/header
	cd res; objcopy --rename-section .data=.header -I binary header -O elf64-x86-64 -B i386 header.o

res/relocations.o: res/relocations
	cd res; objcopy --rename-section .data=.rel.bin -I binary relocations -O elf64-x86-64 -B i386 relocations.o

res/agent_nh.bin.o: res/agent_nh.bin
	cd res; objcopy --rename-section .data=.agent.bin -I binary agent_nh.bin -O elf64-x86-64 -B i386 agent_nh.bin.o

res/agent.0x00000000.wh.o: res/agent.0x00000000.wh
	cd res; objcopy --rename-section .data=.agent.bin -I binary $(notdir $^) -O elf64-x86-64 -B i386 $(notdir $@)

POBJS_AGENT = $(OBJS_BOOTUP) res/relocations.o res/agent.0x00000000.wh.o

pagent.rel: $(POBJS_AGENT) Makefile script-loader 
	g++ -fuse-ld=gold -Wl,--oformat -Wl,binary -Wl,-Map=map.pagent.rel \
	-static -s -nostdlib -fPIE -fpic -Wl,--build-id=none \
	-Wl,--start-group $(POBJS_AGENT) -Wl,--end-group -o $@ -T script-loader

rel_check: res/rel_0x00112000 res/rel_0x13579000 res/rel_0x2648a000 res/rel_0x18375000


res/agent.%.elf res/agent.%.elf.map: $(OBJS_AGENT) $(OBJS_AGENT_EXTRA) libunwind liblzma Makefile
	g++ -fuse-ld=gold \
	-Ttext=$* \
	-fPIE -fpic -Wl,--build-id=none -nostdlib -static \
	-Wl,--start-group $(OBJS_AGENT_EXTRA) $(OBJS_AGENT) $(SOBJS) $(LIBUNWIND) $(LIBLZMA) -Wl,--end-group \
	-Wl,--allow-multiple-definition -Wl,-Map=res/agent.$*.elf.map -o res/agent.$*.elf

res/agent.%.bin res/agent.%.bin.map: header_size res/agent.%.elf $(OBJS_AGENT) $(OBJS_AGENT_EXTRA) libunwind liblzma Makefile
	g++ -fuse-ld=gold -Wl,--oformat -Wl,binary \
	-Ttext=$(call plus, $*, $(shell ./header_size res/agent.$*.elf)) \
	-fPIE -fpic -Wl,--build-id=none -nostdlib -static \
	-Wl,--start-group $(OBJS_AGENT_EXTRA) $(OBJS_AGENT) $(SOBJS) $(LIBUNWIND) $(LIBLZMA) -Wl,--end-group \
	-Wl,--allow-multiple-definition -Wl,-Map=res/agent.$*.bin.map -o res/agent.$*.bin


res/agent.%.wh: res/agent.%.elf res/agent.%.bin
	head -c $(shell ./header_size res/agent.$*.elf) res/agent.$*.elf >$@
	tail -c +$(call plus, $* + 1, $(shell ./header_size res/agent.$*.elf)) res/agent.$*.bin >>$@
    
DEBUG = -O0 -g

$(OBJS_AGENT_CPP): obj/agent/%.o: src/%.cpp
	g++ -c $< -o $@ -fPIC -Ielfio $(DEBUG)
$(OBJS_AGENT_ASM): obj/agent/%.o: src/%.asm
	as -c $< -o $@
$(OBJS_AGENT_EXTRA): obj/agent/%.o: src/%.cpp
	g++ -c $< -o $@ -fPIC $(DEBUG)

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


bin/wallclock: $(OBJS_WALLCLOCK) libagent.so bin
	g++ -o $@ $(OBJS_WALLCLOCK) -lpthread -lunwind -ldl

bin/testprog: src/testprog.cpp obj/largecode.o bin
	g++ src/testprog.cpp obj/largecode.o -o $@ -lpthread

copy_header: src/copy_header.cpp
	g++ -o $@ $^ -Ielfio $(DEBUG)

header_size: src/header_size.cpp
	g++ -o $@ $^ -Ielfio $(DEBUG)

find_relocs: src/find_relocs.cpp
	g++ -o $@ $^ $(DEBUG)

init_agent: init_agent.o syscall.o
	g++ -o $@ $^
