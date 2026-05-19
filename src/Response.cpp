#include "Response.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include "utils.hpp"

Response::Response () {}

Response::~Response () {}

std::string Response::getHeader (const std::string &key) const
{
	std::map<std::string, std::string>::const_iterator it =
		this->headers.find (key);
	if (it != this->headers.end ())
	{
		return it->second;
	}
	return "";
}

std::string Response::getReasonPhrase () const
{
	switch (this->statusCode)
	{
		case 200:
			return ("OK");
		case 301:
			return ("Moved Permanently");
		case 302:
			return ("Found");
		case 303:
			return ("See Other");
		case 307:
			return ("Temporary Redirect");
		case 308:
			return ("Permanent Redirect");
		case 400:
			return ("Bad Request");
		case 403:
			return ("Forbidden");
		case 404:
			return ("Not Found");
		case 405:
			return ("Method Not Allowed");
		case 413:
			return ("Payload Too Large");
		case 500:
			return ("Internal Server Error");
		case 501:
			return ("Not Implemented");
		case 502:
			return ("Bad Gateway");
		case 504:
			return ("Gateway Timeout");
		default:
			return ("Unknown");
	}
}

std::string Response::toString () const
{
	std::string response = "HTTP/1.1 " + intToString (this->statusCode) + " " +
						   getReasonPhrase () + "\r\n";

	for (std::map<std::string, std::string>::const_iterator it =
			 this->headers.begin ();
		 it != this->headers.end (); ++it)
	{
		response += it->first + ": " + it->second + "\r\n";
	}
	response += "\r\n" + this->body;
	return response;
}

void Response::setBodyFromFile (const std::string &path)
{
	std::ifstream file (path.c_str ());

	if (!file)
	{
		std::cerr << "File not found!\n";
	}
	std::ostringstream ss;
	ss << file.rdbuf ();
	this->body = ss.str ();
}
