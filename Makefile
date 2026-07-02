NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g
RM = rm -f

SRC_DIR = src
OBJ_DIR = obj
INC_DIR = include

SRC_FILES = main.cpp WebServ.cpp RequestParser.cpp Request.cpp RequestHandler.cpp RequestInspector.cpp Response.cpp utils.cpp CgiCompletionHandler.cpp CgiHandler.cpp CgiFdRegistry.cpp CgiPipeIO.cpp CgiEventHandler.cpp CgiStartupHandler.cpp CgiTimeoutHandler.cpp Client.cpp ClientEventHandler.cpp ClientResponseApplier.cpp CgiSession.cpp CgiValidator.cpp ConfigLexer.cpp ConfigParser.cpp ConfigValidator.cpp HttpMethod.cpp MimeTypes.cpp StaticFileHandler.cpp ErrorResponseBuilder.cpp RedirectHandler.cpp CgiRequestHandler.cpp ErrorPageResolver.cpp ErrorResponseHandler.cpp PollManager.cpp CgiManager.cpp ChunkedDecoder.cpp UriUtils.cpp PathUtils.cpp Router.cpp RequestDispatcher.cpp StoragePathResolver.cpp UploadHandler.cpp DeleteHandler.cpp HttpMessageUtils.cpp RequestLine.cpp

SRCS = $(addprefix $(SRC_DIR)/, $(SRC_FILES))
OBJS = $(addprefix $(OBJ_DIR)/, $(SRC_FILES:.cpp=.o))
DEPS = $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -I$(INC_DIR) -c $< -o $@

clean:
	$(RM) -r $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re 

-include $(DEPS)
