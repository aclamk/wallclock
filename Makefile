all: wallclock testprog callstep.o loader.o agent.so 

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
	
OBJS = \
	wrapper.o \
	manager.o \
	loader.o \
	callstep.o \
	agent.o \
	largecode.o \
	ptrace.o \
	unix_io.o \
	agent.so

clean:
	rm -f $(OBJS)
wrapper.o: injected/wrapper.asm
	as -c $< -o $@
	
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
     #~/liblzma/src/liblzma/.libs/liblzma.a
    #/usr/lib/x86_64-linux-gnu/libunwind.a ~/liblzma/src/liblzma/.libs/liblzma.so 
    #-nostdlib /usr/lib/x86_64-linux-gnu/libc.a /usr/lib/gcc/x86_64-linux-gnu/6/libstdc++.a /usr/lib/x86_64-linux-gnu/libpthread.a /usr/lib/gcc/x86_64-linux-gnu/6/libgcc_eh.a /usr/lib/x86_64-linux-gnu/libunwind.a ~/liblzma/src/liblzma/.libs/liblzma.a
    
#-Wl,--unresolved-symbols=ignore-all

#-fno-stack-protector 
# -nostdlib 
#/usr/lib/x86_64-linux-gnu/libunwind-x86_64.a
	
#g++ -c $< -o $@
MYOPTS = -fno-omit-frame-pointer
# -fno-exceptions
manager.o: manager.cpp
	g++ -c $< -o $@ $(MYOPTS) -O3 -g

loader.o: loader.cpp
	g++ -c $< -o $@ $(MYOPTS) -O3 -g -fPIC

unix_io.o: unix_io.cpp
	g++ -c $< -o $@ $(MYOPTS) -O3 -g -fPIC

callstep.o: callstep.cpp
	g++ -c $< -o $@ $(MYOPTS) -O3 -g -fPIC

agent.o: agent.cpp
	g++ -c $< -o $@ $(MYOPTS) -O3 -g -fPIC

largecode.o: largecode.cpp
	g++ -c $< -o $@ $(MYOPTS) -O1 -g


testprog: testprog.cpp largecode.o
	g++ $^ -o $@ -lpthread

ptrace.o: ptrace.cpp
	g++ -c $< -o $@ $(MYOPTS) -O3 -g -fPIC
	
#wrapper.o 	
wallclock: ptrace.o callstep.o agent.so manager.o loader.o unix_io.o
	g++ -o $@ $^ -lpthread -lunwind 
	
	
#g++ agent.cpp -fPIE -static -fno-stack-protector -O0 -pie -nostdlib -T script


	