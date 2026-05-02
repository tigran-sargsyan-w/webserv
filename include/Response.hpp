#ifndef RESPONSE_HPP
# define RESPONSE_HPP

#include <string>
#include <map> 

class Response
{
    public:
        Response();
        ~Response();

        int getStatusCode() const { return this->statusCode; }
        std::map<std::string, std::string> getHeaders() const { return this->headers; }
        std::string getBody() const { return this->body; }

        void setStatusCode(int statusCode) { this->statusCode = statusCode; }
        void addHeader(const std::string& key, const std::string& value) { this->headers[key] = value; }
        void setBody(const std::string& body) { this->body = body; }
        void setBodyFromFile(const std::string& path);
        std::string getReasonPhrase() const;
        std::string toString() const;

    private:
        int statusCode;
        std::map<std::string, std::string> headers;
        std::string body;
};

#endif
