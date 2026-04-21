#ifndef REQUEST_HPP
# define REQUEST_HPP

#include <string>
#include <map>

class Request 
{
    public:
        Request();
        ~Request();

        void setMethod(const std::string& method) { _method = method; }
        void setPath(const std::string& path) { _path = path; }
        void setVersion(const std::string& version) { _version = version; }
        void addHeader(const std::string& key, const std::string& value) { _headers[key] = value; }
        void setBody(const std::string& body) { _body = body; }
        void setIsCgi() {_isCgi = true; }

        const std::string& getMethod() const { return _method; }
        const std::string& getPath() const { return _path; }
        const std::string& getVersion() const { return _version; }
        const std::map<std::string, std::string>& getHeaders() const { return _headers; }
        const std::string& getBody() const { return _body; }
        int getIsCgi() const {return _isCgi; }

    private:
        std::string _method;
        std::string _path;
        std::string _version;
        std::map<std::string, std::string> _headers;
        std::string _body;
        bool _isCgi;
};

#endif
