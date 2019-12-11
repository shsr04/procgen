IncludeFlags = -I third-party/range-v3/include -I core
LibFlags =
Libs = -lncurses -lpthread
DefFlags = 

ifeq ($(bigint),1)
	Libs += -lgmpxx -lgmp
	DefFlags += -D USE_GMP
endif

default: generator
.PHONY:

compile_commands.json:
	echo --- Rebuilding $@ ---
	bear make $(patsubst %.cpp, %, $(wildcard *.cpp)) -B

%: %.cpp
	clang++ -std=c++17 -Werror -g -Ofast $(IncludeFlags) $(LibFlags) $(DefFlags) -o $@ $^ $(Libs)
