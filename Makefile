SDIR = src
ODIR = obj
LOBJS = $(ODIR)/smtp.o $(ODIR)/run.o

CC = gcc
CFLAGS = -Wall -O3 -g
LFLAGS = -lpthread

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $< $(LFLAGS)
smtpNum: $(LOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

.PHONY: clean

clean: 
	rm $(ODIR)/*.o
