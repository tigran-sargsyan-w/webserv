#include "RedirectHandler.hpp"
#include "ErrorResponseHandler.hpp"
#include "Utils.hpp"

#include <string>

/**
 * @brief Creates an empty redirect handler.
 */
RedirectHandler::RedirectHandler() {}

/**
 * @brief Copies a redirect handler.
 * @param other - Handler to copy.
 */
RedirectHandler::RedirectHandler(const RedirectHandler &other)
{
    (void)other;
}

/**
 * @brief Assigns a redirect handler.
 * @param other - Handler to assign from.
 * @return Updated handler.
 */
RedirectHandler &RedirectHandler::operator=(const RedirectHandler &other)
{
    (void)other;
    return (*this);
}

/**
 * @brief Destroys the handler.
 */
RedirectHandler::~RedirectHandler() {}

/**
 * @brief Checks whether a status code is a supported redirect code.
 * @param code - HTTP status code.
 * @return True for supported redirect codes.
 */
static bool isRedirectStatusCode(int code)
{
    return (code == 301 || code == 302 || code == 303 || code == 307 || code == 308);
}

/**
 * @brief Builds a redirect response.
 * @param route - Matched route configuration.
 * @param server - Server configuration.
 * @return Redirect response or error response.
 */
Response RedirectHandler::handle(const RouteConfig &route, const ServerConfig &server)
{
    Response response;
    std::string body;

    if (!isRedirectStatusCode(route.returnCode))
        return (ErrorResponseHandler::build(500, "Internal Server Error", server));

    response.setStatusCode(route.returnCode);
    response.addHeader("Location", route.returnPath);
    response.addHeader("Content-Type", "text/html");
    response.addHeader("Connection", "close");

    body = "<html><body><h1>"
        + intToString(route.returnCode)
        + " Redirect</h1><p>Redirecting to "
        + route.returnPath
        + "</p></body></html>";

    response.setBody(body);
    response.addHeader("Content-Length", intToString(body.length()));

    return (response);
}