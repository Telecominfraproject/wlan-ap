# SPDX-License-Identifier: BSD-3-Clause
EXE := tisbl

CPPFLAGS += -I.
CXXFLAGS += -Wwrite-strings -std=gnu++11

SRC := $(wildcard *.cpp)
OBJ := $(patsubst %.cpp,%.o,$(SRC))

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(EXE): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

.PHONY: clean
clean:
	$(RM) $(EXE) $(OBJ)
