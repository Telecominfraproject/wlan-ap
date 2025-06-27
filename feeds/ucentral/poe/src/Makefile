CFLAGS += -Wall -g

INCLUDES =

LDFLAGS = -lubus -lubox -li2c

LIBS = 

SRCS = tps23861-poe-ctrl.c \

OBJS = $(SRCS:.c=.o)

MAIN = tps23861-poe-ctrl


all:   $(MAIN) 

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LDFLAGS) $(LIBS)
	
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN) $(TEST)

