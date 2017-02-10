ODIR = obj
SDIR = src

LOBJS = $(ODIR)/smtp.o $(ODIR)/main.o
DEPS = src/main.h

CC = gcc
CFLAGS = -Wall -O3 -g
LFLAGS = -lpthread

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $< $(LFLAGS)
smtpNum: $(LOBJS) $(DEPS)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

.PHONY: clean

clean: 
	rm $(ODIR)/*.o
