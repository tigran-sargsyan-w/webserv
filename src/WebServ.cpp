#include <iostream>
#include "WebServ.hpp"

WebServ::WebServ()
{
	std::cout << "WebServ created!\n";
}

WebServ::WebServ(const WebServ& other)
{
	(void) other;
	std::cout << "WebServ copy constructor called!\n";
}

WebServ::~WebServ()
{
	std::cout << "WebServ destroyed!\n";
}


WebServ& WebServ::operator=(const WebServ& other)
{
	(void) other;
	std::cout << "WebServ assignement operator called!\n";
	return (*this);
}
