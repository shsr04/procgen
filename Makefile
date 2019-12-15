CompileFlags = -std=c++17 -g -O0 -stdlib=libc++ -Werror -Wconversion -Wmove 
IncludeFlags = -I third-party -I core
LibFlags =
Libs = -lncurses -lSDL2 -lSDL2_ttf
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

%: %.cpp $(wildcard *.hpp)
	clang++ $(CompileFlags) $(IncludeFlags) $(LibFlags) $(DefFlags) -o $@ $< $(Libs)
