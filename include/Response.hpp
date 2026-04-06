#ifndef RESPONSE_HPP
# define RESPONSE_HPP

#include <string>
#include <map> 

class Response
{
    public:
        Response();
        ~Response();

        int getStatusCode() const { return _statusCode; }
        std::map<std::string, std::string> getHeaders() const { return _headers; }
        std::string getBody() const { return _body; }

        void setStatusCode(int statusCode) { _statusCode = statusCode; }
        void addHeader(const std::string& key, const std::string& value) { _headers[key] = value; }
        void setBody(const std::string& body) { _body = body; }
        void setBodyFromFile(const std::string& path);
        std::string getReasonPhrase() const;
        std::string toString() const;

    private:
        int _statusCode;
        std::map<std::string, std::string> _headers;
        std::string _body;


        			// 	std::string response =
					// "HTTP/1.1 200 OK\r\n"
					// "Content-Type: text/html\r\n"
					// "Content-Length: " + ss.str() + "\r\n"
					// "Connection: close\r\n"
					// "\r\n"
					// + body;
        // Add response-related members here (e.g., status code, headers, body)
};

#endif
