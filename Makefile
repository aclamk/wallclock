all: ptrace testprog agent


wrapper.o: wrapper.asm
	as -c $< -o $@
	

agent: agent.cpp Makefile
	g++ -Wl,--start-group agent.cpp -fPIE -static -O0 -pie -T script -nostdlib /usr/lib/x86_64-linux-gnu/libc.a /usr/lib/gcc/x86_64-linux-gnu/6/libgcc_eh.a /usr/lib/x86_64-linux-gnu/libunwind.a ~/liblzma/src/liblzma/.libs/liblzma.a  -Wl,--end-group 
#-Wl,--unresolved-symbols=ignore-all

#-fno-stack-protector 
# -nostdlib 
#/usr/lib/x86_64-linux-gnu/libunwind-x86_64.a
	
#g++ -c $< -o $@

testprog: testprog.cpp largecode.o
	g++ $^ -o $@ 

#tightloop: tightloop.cpp
#	g++ tightloop.cpp -o tightloop

ptrace.o: ptrace.cpp
	g++ -c $< -o $@ -fno-omit-frame-pointer -O0 -g
	
ptrace: ptrace.o wrapper.o
	g++ -o ptrace $^ -lpthread -lunwind
	
	
#g++ agent.cpp -fPIE -static -fno-stack-protector -O0 -pie -nostdlib -T script


	