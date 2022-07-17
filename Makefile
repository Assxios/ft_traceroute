NAME			=	ft_traceroute

SOURCE_FOLDER	=	./src
SOURCES			=	main.c \
					args.c \
					network.c \
					icmp.c \
					utils.c

OBJECT_FOLDER	=	./obj
OBJECTS			=	$(SOURCES:%.c=$(OBJECT_FOLDER)/%.o)

INCLUDE_FOLDER	=	./include
INCLUDES		=	types.h

COMPILER		=	gcc

COMPILER_FLAGS	=	-I$(INCLUDE_FOLDER)
COMPILER_FLAGS	+=	-Wall -Wextra -Werror -Wno-implicit-fallthrough
COMPILER_FLAGS	+=	-g3

FLAGS			=	google.com

all: $(NAME)

$(OBJECT_FOLDER):
	mkdir -p $@

$(OBJECT_FOLDER)/%.o: $(SOURCE_FOLDER)/%.c $(INCLUDES:%.h=$(INCLUDE_FOLDER)/%.h)
	$(COMPILER) $(COMPILER_FLAGS) -c $< -o $@

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
	valgrind --leak-check=full --show-reachable=yes make run

.PHONY: all clean fclean re run
