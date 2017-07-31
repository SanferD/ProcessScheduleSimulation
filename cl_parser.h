#ifndef CL_PARSER_H
#define CL_PARSER_H

#include <algorithm>
#include <string>
#include <vector>

class CLParser
{
private: 
	struct pair_t 
	{
		std::string option;
		std::string value;
	};
	std::vector<pair_t> pairs;

	pair_t split(std::string input, std::string token);
public:
	CLParser (int &argc, char **argv);
	bool optionExists(const std::string &option) const;
	std::string optionValue(const std::string &option) const;
};

#endif