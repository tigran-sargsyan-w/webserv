#include "RedirectHandler.hpp"
#include "ErrorResponseHandler.hpp"
#include "utils.hpp"

#include <string>

RedirectHandler::RedirectHandler() {}

RedirectHandler::RedirectHandler(const RedirectHandler &other)
{
    (void)other;
}

RedirectHandler &RedirectHandler::operator=(const RedirectHandler &other)
{
    (void)other;
    return (*this);
}

RedirectHandler::~RedirectHandler() {}

static bool isRedirectStatusCode(int code)
{
    return (code == 301 || code == 302 || code == 303 || code == 307 || code == 308);
}

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