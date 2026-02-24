#include "HttpResponse.hpp"
#include "Utils.hpp"
#include <sstream>

HttpResponse::HttpResponse() : statusCode(200) {}

void HttpResponse::setStatus(int code) { statusCode = code; }

void HttpResponse::setHeader(const std::string& name, const std::string& value) {
    headers[name] = value;
}

void HttpResponse::setBody(const std::string& b) {
    body = b;
    headers["Content-Length"] = Utils::intToStr((long long)b.size());
}

void HttpResponse::setBody(const std::string& b, const std::string& contentType) {
    body = b;
    headers["Content-Type"]   = contentType;
    headers["Content-Length"] = Utils::intToStr((long long)b.size());
}

std::string HttpResponse::build() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode << " " << statusText(statusCode) << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it) {
        oss << it->first << ": " << it->second << "\r\n";
    }
    oss << "\r\n";
    oss << body;
    return oss.str();
}

std::string HttpResponse::statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

std::string HttpResponse::getMimeType(const std::string& ext) {
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".pdf")  return "application/pdf";
    if (ext == ".txt")  return "text/plain";
    if (ext == ".xml")  return "application/xml";
    if (ext == ".zip")  return "application/zip";
    return "application/octet-stream";
}
