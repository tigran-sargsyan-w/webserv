NAME    = webserv
CXX     = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
INCLUDES = -I include

SRCS =  src/main.cpp \
        src/Config.cpp \
        src/ConfigParser.cpp \
        src/HttpRequest.cpp \
        src/HttpResponse.cpp \
        src/Router.cpp \
        src/CgiHandler.cpp \
        src/Connection.cpp \
        src/EventLoop.cpp \
        src/Server.cpp \
        src/Utils.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
