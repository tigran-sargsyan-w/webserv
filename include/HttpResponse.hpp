#pragma once
#include <string>
#include <map>

class HttpResponse {
public:
    HttpResponse();

    void setStatus(int code);
    void setHeader(const std::string& name, const std::string& value);
    void setBody(const std::string& body);
    void setBody(const std::string& body, const std::string& contentType);

    std::string build() const;

    static std::string statusText(int code);
    static std::string getMimeType(const std::string& ext);

    int statusCode;
    std::map<std::string, std::string> headers;
    std::string body;
};
