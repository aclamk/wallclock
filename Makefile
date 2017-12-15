all: ptrace testprog agent callstep.o loader.o agent.so


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
	

agent: agent.cpp Makefile wrapper.o callstep.o unix_io.o
	g++ -Wl,--start-group agent.cpp wrapper.o callstep.o unix_io.o -fPIE -static -O0 -pie -T script -nostdlib /usr/lib/x86_64-linux-gnu/libc.a /usr/lib/gcc/x86_64-linux-gnu/6/libstdc++.a /usr/lib/x86_64-linux-gnu/libpthread.a /usr/lib/gcc/x86_64-linux-gnu/6/libgcc_eh.a /usr/lib/x86_64-linux-gnu/libunwind.a ~/liblzma/src/liblzma/.libs/liblzma.a  -Wl,--end-group 

agent.so: agent.o wrapper.o callstep.o unix_io.o
	g++ -shared -Wl,-export-dynamic -Wl,-soname,agent.so \
    -o agent.so agent.o wrapper.o callstep.o unix_io.o /usr/lib/x86_64-linux-gnu/libunwind.a ~/liblzma/src/liblzma/.libs/liblzma.so -pthread #~/liblzma/src/liblzma/.libs/liblzma.a
    #-nostdlib /usr/lib/x86_64-linux-gnu/libc.a /usr/lib/gcc/x86_64-linux-gnu/6/libstdc++.a /usr/lib/x86_64-linux-gnu/libpthread.a /usr/lib/gcc/x86_64-linux-gnu/6/libgcc_eh.a /usr/lib/x86_64-linux-gnu/libunwind.a ~/liblzma/src/liblzma/.libs/liblzma.a
    
#-Wl,--unresolved-symbols=ignore-all

#-fno-stack-protector 
# -nostdlib 
#/usr/lib/x86_64-linux-gnu/libunwind-x86_64.a
	
#g++ -c $< -o $@

manager.o: manager.cpp
	g++ -c $< -o $@ -fno-omit-frame-pointer -O0 -g

loader.o: loader.cpp
	g++ -c $< -o $@ -fno-omit-frame-pointer -O0 -g -fPIC

unix_io.o: unix_io.cpp
	g++ -c $< -o $@ -fno-omit-frame-pointer -O0 -g -fPIC


callstep.o: callstep.cpp
	g++ -c $< -o $@ -fno-omit-frame-pointer -O0 -g -fPIC

agent.o: agent.cpp
	g++ -c $< -o $@ -fno-omit-frame-pointer -O0 -g -fPIC

largecode.o: largecode.cpp
	g++ -c $< -o $@ -fno-omit-frame-pointer -O0 -g


testprog: testprog.cpp largecode.o
	g++ $^ -o $@ 

#tightloop: tightloop.cpp
#	g++ tightloop.cpp -o tightloop

ptrace.o: ptrace.cpp
	g++ -c $< -o $@ -fno-omit-frame-pointer -O0 -g -fPIC
	
#wrapper.o 	
ptrace: ptrace.o callstep.o agent.so manager.o loader.o unix_io.o
	g++ -o ptrace $^ -lpthread -lunwind 
	
	
#g++ agent.cpp -fPIE -static -fno-stack-protector -O0 -pie -nostdlib -T script


	