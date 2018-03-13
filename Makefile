obj = main.o
bin = test

CFLAGS = -pedantic -Wall -g
LDFLAGS = -lGL -lglut

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)