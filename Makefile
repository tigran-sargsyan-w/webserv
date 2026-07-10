# **************************************************************************** #
#                                  Makefile                                    #
# **************************************************************************** #

NAME := webserv

CXX := c++
RM := rm -rf

LOG_LEVEL ?= 1

CXXFLAGS := -Wall -Wextra -Werror -std=c++98 -g
DEPFLAGS := -MMD -MP

SRC_DIR := src
OBJ_DIR := obj
INC_DIR := include

# -------------------------------
# Include directories
# -------------------------------

MODULES := \
	core \
	config \
	http \
	network \
	routing \
	handlers \
	storage \
	session \
	cgi \
	utils

INCLUDE_DIRS := \
	$(INC_DIR) \
	$(addprefix $(INC_DIR)/,$(MODULES))

INCLUDES := $(addprefix -I,$(INCLUDE_DIRS))

CPPFLAGS := \
	-DWEBSERV_LOG_LEVEL=$(LOG_LEVEL) \
	$(INCLUDES)

# -------------------------------
# Source files
# -------------------------------

CORE_SRCS := \
	core/Logger.cpp \
	core/WebServ.cpp

CONFIG_SRCS := \
	config/ConfigLexer.cpp \
	config/ConfigParser.cpp \
	config/ConfigValidator.cpp

HTTP_SRCS := \
	http/ChunkedDecoder.cpp \
	http/HttpMessageUtils.cpp \
	http/HttpMethod.cpp \
	http/MimeTypes.cpp \
	http/Request.cpp \
	http/RequestInspector.cpp \
	http/RequestLine.cpp \
	http/RequestParser.cpp \
	http/Response.cpp

NETWORK_SRCS := \
	network/Client.cpp \
	network/ClientEventHandler.cpp \
	network/ClientResponseApplier.cpp \
	network/ClientTimeoutHandler.cpp \
	network/ConnectionManager.cpp \
	network/ListenerSocketHandler.cpp \
	network/PollEventHandler.cpp \
	network/PollManager.cpp

ROUTING_SRCS := \
	routing/RequestDispatcher.cpp \
	routing/RequestHandler.cpp \
	routing/Router.cpp

HANDLERS_SRCS := \
	handlers/DeleteHandler.cpp \
	handlers/ErrorPageResolver.cpp \
	handlers/ErrorResponseBuilder.cpp \
	handlers/ErrorResponseHandler.cpp \
	handlers/RedirectHandler.cpp \
	handlers/StaticFileHandler.cpp \
	handlers/TemplateRenderer.cpp

STORAGE_SRCS := \
	storage/MultipartParser.cpp \
	storage/StoragePathResolver.cpp \
	storage/UploadHandler.cpp \
	storage/UploadStorage.cpp

SESSION_SRCS := \
	session/CookieParser.cpp \
	session/SessionHandler.cpp \
	session/SessionManager.cpp

CGI_SRCS := \
	cgi/CgiCompletionHandler.cpp \
	cgi/CgiEventHandler.cpp \
	cgi/CgiFdRegistry.cpp \
	cgi/CgiHandler.cpp \
	cgi/CgiManager.cpp \
	cgi/CgiPipeIO.cpp \
	cgi/CgiRequestHandler.cpp \
	cgi/CgiSession.cpp \
	cgi/CgiStartupHandler.cpp \
	cgi/CgiTimeoutHandler.cpp \
	cgi/CgiValidator.cpp

UTILS_SRCS := \
	utils/utils.cpp \
	utils/PathUtils.cpp \
	utils/UriUtils.cpp

SRC_FILES := \
	main.cpp \
	$(CORE_SRCS) \
	$(CONFIG_SRCS) \
	$(HTTP_SRCS) \
	$(NETWORK_SRCS) \
	$(ROUTING_SRCS) \
	$(HANDLERS_SRCS) \
	$(STORAGE_SRCS) \
	$(SESSION_SRCS) \
	$(CGI_SRCS) \
	$(UTILS_SRCS)

SRCS := $(addprefix $(SRC_DIR)/,$(SRC_FILES))
OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))
DEPS := $(OBJS:.o=.d)

# **************************************************************************** #
#                                 Build Rules                                  #
# **************************************************************************** #

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re