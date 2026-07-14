# **************************************************************************** #
#                                  Makefile                                    #
# **************************************************************************** #

NAME := webserv

CXX := c++
RM := rm -rf

LOG_LEVEL ?= 1

# -------------------------------
# Log level description
# -------------------------------

ifeq ($(LOG_LEVEL),1)
LOG_LEVEL_DESC := errors only
else ifeq ($(LOG_LEVEL),2)
LOG_LEVEL_DESC := errors + info
else ifeq ($(LOG_LEVEL),3)
LOG_LEVEL_DESC := errors + info + debug
else
LOG_LEVEL_DESC := custom
endif

# -------------------------------
# Compiler flags
# -------------------------------

CXXFLAGS := -Wall -Wextra -Werror -std=c++98 -g
DEPFLAGS := -MMD -MP

SRC_DIR := src
OBJ_DIR := obj
INC_DIR := include

LOG_LEVEL_FILE := $(OBJ_DIR)/.log_level
BUILD_MARKER := $(OBJ_DIR)/.build_started

# -------------------------------
# Color codes
# -------------------------------

RESET := \033[0m
BOLD := \033[1m
RED := \033[31m
GREEN := \033[32m
YELLOW := \033[33m
MAGENTA := \033[35m
CYAN := \033[36m

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
	utils/Utils.cpp \
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

all:
	@printf "$(BOLD)Webserv build$(RESET)\n"
	@printf "$(CYAN)[CONFIG]$(RESET) Target: $(NAME)\n"
	@printf "$(CYAN)[CONFIG]$(RESET) Logger level: $(LOG_LEVEL)"
	@printf " ($(LOG_LEVEL_DESC))\n"
	@printf "$(YELLOW)[CHECK]$(RESET) Resolving build dependencies\n"
	@$(RM) $(BUILD_MARKER)
	@$(MAKE) --no-print-directory $(NAME)
	@$(RM) $(BUILD_MARKER)
	@printf "$(GREEN)[DONE]$(RESET) $(BOLD)$(NAME)$(RESET) is ready\n"

$(LOG_LEVEL_FILE): FORCE
	@mkdir -p $(OBJ_DIR)
	@if [ ! -f $@ ]; then \
		printf "%s\n" "$(LOG_LEVEL)" > $@; \
	elif [ "$$(cat $@)" != "$(LOG_LEVEL)" ]; then \
		old_level="$$(cat $@)"; \
		printf "$(MAGENTA)[CONFIG]$(RESET) "; \
		printf "Logger level changed: %s -> $(LOG_LEVEL)\n" "$$old_level"; \
		printf "%s\n" "$(LOG_LEVEL)" > $@; \
	fi

$(NAME): $(OBJS)
	@printf "$(YELLOW)[LINK]$(RESET) Creating $(BOLD)$(NAME)$(RESET)\n"
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp Makefile $(LOG_LEVEL_FILE)
	@mkdir -p $(@D)
	@if mkdir $(BUILD_MARKER) 2>/dev/null; then \
		printf "$(YELLOW)[BUILD]$(RESET) Compiling source files\n"; \
	fi
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	@printf "$(RED)[CLEAN]$(RESET) Removing object files\n"
	@$(RM) $(OBJ_DIR)

fclean: clean
	@printf "$(RED)[FCLEAN]$(RESET) Removing $(NAME)\n"
	@$(RM) $(NAME)

re: fclean
	@$(MAKE) --no-print-directory all LOG_LEVEL=$(LOG_LEVEL)

FORCE:

-include $(DEPS)

.PHONY: all clean fclean re FORCE
