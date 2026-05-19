#include "ErrorResponseBuilder.hpp"
#include "utils.hpp"

#include <sstream>
#include <string>

ErrorResponseBuilder::ErrorResponseBuilder () {}

ErrorResponseBuilder::ErrorResponseBuilder (const ErrorResponseBuilder &other)
{
	(void)other;
}

ErrorResponseBuilder &ErrorResponseBuilder::operator= (const ErrorResponseBuilder &other)
{
	(void)other;
	return (*this);
}

ErrorResponseBuilder::~ErrorResponseBuilder () {}

Response ErrorResponseBuilder::build (int statusCode, const std::string &message)
{
	Response res;
	std::stringstream ss;

	res.setStatusCode (statusCode);
	ss << "<html><body><h1>" << statusCode << " " << message << "</h1></body></html>";
	res.setBody (ss.str ());
	return res;
}