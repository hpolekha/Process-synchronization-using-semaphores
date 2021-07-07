COMPILER=gcc
OUTPUT=proc
FLAGS=-Werror -pthread -std=gnu99

all: semafory.c
	$(COMPILER) semafory.c -o $(OUTPUT) $(FLAGS)
	./$(OUTPUT)

clean:
	rm proc
