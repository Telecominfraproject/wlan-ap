# Executable
EXE    = tisbl
 
# Compiler, Linker Defines
#CC      = g++ 
#CFLAGS  = -w -O -Wno-deprecated -I. -fpermissive -std=gnu++0x
#CFLAGS  = -I. -fpermissive -Wwrite-strings -lstdc++
CFLAGS  = -I. -Wwrite-strings -std=gnu++0x -lstdc++ -fPIC
#CFLAGS  = -I. -lstdc++ -Wwrite-strings -std=gnu++0x
#LIBS    = 
#LDFLAGS = $(LIBS)
 
# Compile and Assemble C++ Source Files into Object Files
%.o: %.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -c $< -o $@
# Source and Object files
SRC    = $(wildcard *.cpp)
OBJ    = $(patsubst %.cpp, %.o, $(SRC))
	
# Link all Object Files with external Libraries into Binaries
$(EXE): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $(EXE)
#	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $(EXE) -lz
 
.PHONY: clean
clean:
	 -rm -f $(EXE) *.o
