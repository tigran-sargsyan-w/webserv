#ifndef REQUEST_HPP
# define REQUEST_HPP

#include <string>
#include <map>

class Request 
{
    public:
        Request();
        ~Request();

        void setMethod(const std::string& method) { this->method = method; };
        void setPath(const std::string& path) { this->path = path; };
        void setVersion(const std::string& version) { this->version = version; };
        void addHeader(const std::string& key, const std::string& value) { this->headers[key] = value; };
        void setBody(const std::string& body) { this->body = body; };
        void setIsCgi() { this->isCgi = true; };
        void setValid() { this->valid = true; };

        const std::string& getMethod() const { return this->method; };
        const std::string& getPath() const { return this->path; };
        const std::string& getVersion() const { return this->version; };
        const std::map<std::string, std::string>& getHeaders() const { return this->headers; };
        const std::string& getBody() const { return this->body; };
        bool isValid() const { return this->valid; };
        int getIsCgi() const { return this->isCgi; };

    private:
        std::string method;
        std::string path;
        std::string version;
        std::map<std::string, std::string> headers;
        std::string body;
        bool valid;
        bool isCgi;
};

#endif
