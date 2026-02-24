#pragma once
#include <string>
#include <map>
#include <vector>

enum ParseState {
    PS_REQUEST_LINE,
    PS_HEADERS,
    PS_BODY,
    PS_CHUNK_SIZE,
    PS_CHUNK_DATA,
    PS_CHUNK_TRAILER,
    PS_COMPLETE,
    PS_ERROR
};

class HttpRequest {
public:
    HttpRequest();

    // Feed data into the parser. Returns bytes consumed, or -1 on error.
    int feed(const char* data, int len);

    bool isComplete() const;
    bool isError() const;
    int getErrorCode() const;

    std::string method;
    std::string uri;
    std::string path;
    std::string queryString;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;

    long long contentLength;
    bool chunked;
    long long maxBodySize;

private:
    ParseState state_;
    std::string buf_;
    int errorCode_;
    long long bodyBytesRead_;
    long long chunkSize_;
    long long chunkBytesRead_;

    bool parseRequestLine(const std::string& line);
    bool parseHeaderLine(const std::string& line);
    void parseUri();
    bool processBuffer();
    bool processChunkSize();
    bool processChunkData();
};
