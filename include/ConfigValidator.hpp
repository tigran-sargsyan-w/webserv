#ifndef CONFIGVALIDATOR_HPP
#define CONFIGVALIDATOR_HPP

#include "Config.hpp"

class ConfigValidator
{
	private:
		ConfigValidator();

	public:
		static void validate(const Config &config);
		static void debugPrintValidation(const Config &config);
};

#endif