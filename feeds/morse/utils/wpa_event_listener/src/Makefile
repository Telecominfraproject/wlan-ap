#
# Copyright (C) 2023-2024 Morse Micro Pty Ltd. All rights reserved
#
APP=wpa_event_listener

LDFLAGS+=-lwpa_client -lubox

ODIR=build

_OBJ = wpa_event_listener.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c
	mkdir $(ODIR) -p
	$(CC) -c -o $@ $< $(CFLAGS)

$(APP): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -rf $(ODIR) $(APP)