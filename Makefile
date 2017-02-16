SDIR = src
LOBJS = $(SDIR)/smtp.o $(SDIR)/main.o

CC = gcc
CFLAGS = -Wall -O3 -g
LFLAGS = -lpthread

$(SDIR)/%.o: $(SDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $< $(LFLAGS)
smtpNum: $(LOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

.PHONY: clean

clean: 
	rm $(SDIR)/*.o
