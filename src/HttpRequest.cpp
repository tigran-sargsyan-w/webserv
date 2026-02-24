#include "HttpRequest.hpp"
#include "Utils.hpp"
#include <cstdlib>
#include <cstring>
#include <sstream>

static const size_t MAX_HEADER_SIZE = 16384; // 16 KB

HttpRequest::HttpRequest()
    : contentLength(-1), chunked(false), maxBodySize(10LL * 1024 * 1024),
      state_(PS_REQUEST_LINE), errorCode_(0),
      bodyBytesRead_(0), chunkSize_(0), chunkBytesRead_(0) {}

bool HttpRequest::isComplete() const { return state_ == PS_COMPLETE; }
bool HttpRequest::isError()    const { return state_ == PS_ERROR; }
int  HttpRequest::getErrorCode() const { return errorCode_; }

// ---------------------------------------------------------------------------

int HttpRequest::feed(const char* data, int len) {
    if (state_ == PS_COMPLETE || state_ == PS_ERROR) return 0;
    buf_.append(data, len);
    if (!processBuffer()) return -1;
    return len;
}

// ---------------------------------------------------------------------------

bool HttpRequest::processBuffer() {
    while (true) {
        if (state_ == PS_REQUEST_LINE) {
            size_t pos = buf_.find("\r\n");
            if (pos == std::string::npos) {
                if (buf_.size() > MAX_HEADER_SIZE) { errorCode_ = 400; state_ = PS_ERROR; return false; }
                return true;
            }
            std::string line = buf_.substr(0, pos);
            buf_.erase(0, pos + 2);
            if (!parseRequestLine(line)) return false;
            state_ = PS_HEADERS;
        } else if (state_ == PS_HEADERS) {
            size_t pos = buf_.find("\r\n");
            if (pos == std::string::npos) {
                if (buf_.size() > MAX_HEADER_SIZE) { errorCode_ = 400; state_ = PS_ERROR; return false; }
                return true;
            }
            std::string line = buf_.substr(0, pos);
            buf_.erase(0, pos + 2);
            if (line.empty()) {
                // blank line -> end of headers
                // determine body reading mode
                std::map<std::string, std::string>::iterator it = headers.find("transfer-encoding");
                if (it != headers.end() && Utils::toLower(it->second).find("chunked") != std::string::npos) {
                    chunked = true;
                    contentLength = -1;
                    state_ = PS_CHUNK_SIZE;
                } else {
                    it = headers.find("content-length");
                    if (it != headers.end()) {
                        contentLength = Utils::strToLong(it->second);
                        if (contentLength < 0) { errorCode_ = 400; state_ = PS_ERROR; return false; }
                        if (maxBodySize >= 0 && contentLength > maxBodySize) {
                            errorCode_ = 413; state_ = PS_ERROR; return false;
                        }
                        if (contentLength == 0) { state_ = PS_COMPLETE; return true; }
                        state_ = PS_BODY;
                    } else {
                        // no body
                        state_ = PS_COMPLETE;
                        return true;
                    }
                }
            } else {
                if (!parseHeaderLine(line)) return false;
            }
        } else if (state_ == PS_BODY) {
            long long remaining = contentLength - bodyBytesRead_;
            if (remaining <= 0) { state_ = PS_COMPLETE; return true; }
            long long take = (long long)buf_.size();
            if (take > remaining) take = remaining;
            body.append(buf_, 0, (size_t)take);
            buf_.erase(0, (size_t)take);
            bodyBytesRead_ += take;
            if (bodyBytesRead_ >= contentLength) { state_ = PS_COMPLETE; return true; }
            return true;
        } else if (state_ == PS_CHUNK_SIZE) {
            if (!processChunkSize()) return false;
        } else if (state_ == PS_CHUNK_DATA) {
            if (!processChunkData()) return false;
        } else if (state_ == PS_CHUNK_TRAILER) {
            // consume trailing \r\n after chunk data
            size_t pos = buf_.find("\r\n");
            if (pos == std::string::npos) return true;
            buf_.erase(0, pos + 2);
            state_ = PS_CHUNK_SIZE;
        } else {
            return true;
        }
    }
}

bool HttpRequest::processChunkSize() {
    size_t pos = buf_.find("\r\n");
    if (pos == std::string::npos) return true;
    std::string hexStr = buf_.substr(0, pos);
    buf_.erase(0, pos + 2);
    // strip chunk extensions
    size_t semi = hexStr.find(';');
    if (semi != std::string::npos) hexStr = hexStr.substr(0, semi);
    hexStr = Utils::trim(hexStr);
    if (hexStr.empty()) { errorCode_ = 400; state_ = PS_ERROR; return false; }
    chunkSize_ = (long long)strtoll(hexStr.c_str(), NULL, 16);
    chunkBytesRead_ = 0;
    if (chunkSize_ == 0) {
        // last chunk - consume trailing \r\n
        size_t end = buf_.find("\r\n");
        if (end != std::string::npos) buf_.erase(0, end + 2);
        state_ = PS_COMPLETE;
        return true;
    }
    if (maxBodySize >= 0 && (long long)body.size() + chunkSize_ > maxBodySize) {
        errorCode_ = 413; state_ = PS_ERROR; return false;
    }
    state_ = PS_CHUNK_DATA;
    return true;
}

bool HttpRequest::processChunkData() {
    long long remaining = chunkSize_ - chunkBytesRead_;
    if (remaining <= 0) { state_ = PS_CHUNK_TRAILER; return true; }
    long long take = (long long)buf_.size();
    if (take > remaining) take = remaining;
    body.append(buf_, 0, (size_t)take);
    buf_.erase(0, (size_t)take);
    chunkBytesRead_ += take;
    if (chunkBytesRead_ >= chunkSize_) {
        state_ = PS_CHUNK_TRAILER;
    }
    return true;
}

bool HttpRequest::parseRequestLine(const std::string& line) {
    size_t sp1 = line.find(' ');
    if (sp1 == std::string::npos) { errorCode_ = 400; state_ = PS_ERROR; return false; }
    size_t sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) { errorCode_ = 400; state_ = PS_ERROR; return false; }

    method  = line.substr(0, sp1);
    uri     = line.substr(sp1 + 1, sp2 - sp1 - 1);
    version = line.substr(sp2 + 1);

    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        errorCode_ = 505; state_ = PS_ERROR; return false;
    }
    if (method != "GET" && method != "POST" && method != "DELETE"
        && method != "HEAD" && method != "PUT" && method != "OPTIONS") {
        errorCode_ = 501; state_ = PS_ERROR; return false;
    }
    parseUri();
    return true;
}

bool HttpRequest::parseHeaderLine(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) { errorCode_ = 400; state_ = PS_ERROR; return false; }
    std::string name  = Utils::toLower(Utils::trim(line.substr(0, colon)));
    std::string value = Utils::trim(line.substr(colon + 1));
    headers[name] = value;
    return true;
}

void HttpRequest::parseUri() {
    size_t qmark = uri.find('?');
    if (qmark != std::string::npos) {
        path        = uri.substr(0, qmark);
        queryString = uri.substr(qmark + 1);
    } else {
        path        = uri;
        queryString = "";
    }
    path = Utils::urlDecode(path);
    path = Utils::normalizePath(path);
}
