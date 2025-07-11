EXE=htproxy

$(EXE): main.c proxy.c
	cc -Wall -o $@ $^

format:
	clang-format -style=file -i *.c

clean:
	rm -f $(EXE) *.o
