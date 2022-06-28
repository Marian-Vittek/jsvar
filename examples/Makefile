none:;
	@echo
	@echo "Use 'make e<n> run' to compile and run the example <n> where <n> is one of 1,2,3,..."
	@echo

one: e1

all: e1 e2 e3 e4 e5 e6 e7 e8 e9

e1: e1.c ../src/jsvar.h
	gcc -g e1.c ../src/jsvar.c -I../src -DJSVAR_USE_OPEN_SSL -lssl -lcrypto 

e2: e2.c ../src/jsvar.h
	gcc -g e2.c ../src/jsvar.c -I../src -DJSVAR_USE_OPEN_SSL -lssl -lcrypto 

e3: e3.c ../src/jsvar.h
	gcc -g e3.c ../src/jsvar.c -I../src -DJSVAR_USE_OPEN_SSL -lssl -lcrypto 

e4: e4.c ../src/jsvar.h
	gcc -g e4.c ../src/jsvar.c -I../src -DJSVAR_USE_OPEN_SSL -lssl -lcrypto 

e5: e5.c ../src/jsvar.h
	gcc -g e5.c ../src/jsvar.c -I../src -DJSVAR_USE_OPEN_SSL -lssl -lcrypto 

e6: e6.c ../src/jsvar.h
	gcc -g e6.c ../src/jsvar.c -I../src -DJSVAR_USE_OPEN_SSL -DJSVAR_ALL_LIBRARIES -lssl -lcrypto 

e7: e7.c ../src/jsvar.h
	gcc -g e7.c ../src/jsvar.c -I../src -DJSVAR_USE_OPEN_SSL -DJSVAR_USE_WCHAR_T -lssl -lcrypto 

e8: e8.c ../src/jsvar.h
	gcc -g e8.c ../src/jsvar.c -I../src

e9: e9.c ../src/jsvar.h
	gcc -g e9.c ../src/jsvar.c -I../src -DJSVAR_USE_OPEN_SSL -DJSVAR_ALL_LIBRARIES -lssl -lcrypto 


run:;
	(ulimit -c unlimited; ./a.out)
	