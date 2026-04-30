#ifndef REQUEST_INSPECTOR_HPP
# define REQUEST_INSPECTOR_HPP

#include <string>

enum InspectRequestStatus
{
  EMPTY,
  NEED_MORE_DATA,
  COMPLETED,
  REQUEST_TOO_LARGE,
  BAD_REQUEST = 400,
  URI_TOO_LONG = 414,
  HEADER_TOO_LARGE = 431,
  NOT_IMPLEMENTED = 501,
  INVALID
};

class RequestInspector
{
  public:
    RequestInspector() : status(EMPTY), requestLineValid(false) {};
    ~RequestInspector() {};

    InspectRequestStatus inspectRequest(const std::string& rawRequest);
    InspectRequestStatus status;

  private:
    void inspectRequestLine(const std::string& requestLine);
    bool requestLineValid;
};

#endif
