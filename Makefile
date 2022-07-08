NAME			=	ft_traceroute

SOURCE_FOLDER	=	./src
SOURCES			=	main.c

OBJECT_FOLDER	=	./obj
OBJECTS			=	$(SOURCES:%.c=$(OBJECT_FOLDER)/%.o)

COMPILER		=	gcc -Wall -Wextra -Werror -g3

FLAGS			=	google.com

all: $(NAME)

$(OBJECT_FOLDER):
	mkdir -p $@

$(OBJECT_FOLDER)/%.o: $(SOURCE_FOLDER)/%.c
	$(COMPILER) -c $< -o $@

$(NAME): $(OBJECT_FOLDER) $(OBJECTS)
	$(COMPILER) -o $@ $(OBJECTS)

clean:
	rm -rf $(OBJECT_FOLDER)

fclean: clean
	rm -rf $(NAME)

re: fclean all

run: $(NAME)
	sudo ./$(NAME) $(FLAGS)

valgrind:
	valgrind make run

.PHONY: all clean fclean re run
