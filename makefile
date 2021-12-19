CC=gcc
CFLAGS=--std=c17 -Wextra -Werror -pedantic -g
SRCDIR=src
BUILDDIR=build
EXEC=dbview
LD=gcc

$(EXEC): $(BUILDDIR)/main.o
	$(LD) $^ -o $(EXEC)

$(BUILDDIR):
	mkdir $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -rf $(BUILDDIR)
	rm -f $(EXEC)