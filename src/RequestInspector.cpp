#include "RequestInspector.hpp"
#include <string>
#include <sstream>

const std::size_t MAX_HEADERS_SIZE = 32768;
const std::size_t MAX_REQUEST_LINE_SIZE = 8192;
// const std::size_t MAX_HEADER_FIELD_SIZE = 8192;

void RequestInspector::inspectRequestLine(const std::string& requestLine)
{
  if (requestLine.size() > MAX_REQUEST_LINE_SIZE)
  {
    this->status = URI_TOO_LONG;
    return;
  }

  std::stringstream ss;
  ss << requestLine;
  std::string method;
  std::string uri;
  std::string version;
  std::string extra;

  if (!(ss >> method >> uri >> version))
  {
    this->status = BAD_REQUEST;
    return;
  }
  if (ss >> extra)
  {
    this->status = BAD_REQUEST;
    return;
  }
  if (method != "GET" && method != "POST" && method != "DELETE")
  {
    this->status = NOT_IMPLEMENTED;
    return;
  }
  if (uri.empty() || uri[0] != '/')
  {
    this->status = BAD_REQUEST;
    return;
  }
  if (version != "HTTP/1.1" && version != "HTTP/1.0")
  {
    this->status = BAD_REQUEST;
    return;
  }
  requestLineValid = true;
  this->status = COMPLETED;
}

InspectRequestStatus RequestInspector::inspectRequest(const std::string& rawRequest)
{

  std::string requestLine;
  std::stringstream ss;

  ss << rawRequest;
  std::getline(ss, requestLine);

  std::size_t headersEnd = rawRequest.find("\r\n\r\n");
  if (headersEnd == std::string::npos)
    this->status = NEED_MORE_DATA;
  else if (headersEnd > MAX_HEADERS_SIZE)
  {
    this->status = HEADER_TOO_LARGE;
    return this->status;
  }

  inspectRequestLine(requestLine);

  return status;
}
