EXEC = ftnl
SRC = api.c ftnl.c

all:$(EXEC)
$(EXEC):$(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC) $(LDLIBS) $(LIBS)

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o
