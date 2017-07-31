#include "cl_parser.h"
#include <assert.h>

typedef std::vector<CLParser::pair_t>::const_iterator parser_vec_iter_t;

/* local split member function */
CLParser::pair_t 
CLParser::split(std::string input, std::string token)
{
  CLParser::pair_t pair;
  size_t pos;

	/* get the option name */
  pos = input.find(token);
  pair.option = input.substr(0, pos);

  /* if option has value, get it */
  if (pos != std::string::npos)
		pair.value = input.substr(pos+token.size(), std::string::npos);
	else
		pair.value = std::string("");

	/* return the pair */
	return pair;
}

CLParser::CLParser(int &argc, char **argv)
{
	std::string token = "=";

	for (int i=0; i!=argc; i++)
	{
		std::string arg(argv[i]);
		pairs.push_back(split(arg, token));
	}
}

bool
CLParser::optionExists(const std::string &option) const
{
	parser_vec_iter_t iter = pairs.begin();
	for (iter=pairs.begin(); iter!=pairs.end(); iter++)
		if (iter->option.compare(option) == 0)
			return true;

	return false;
}

std::string
CLParser::optionValue(const std::string &option) const
{
	parser_vec_iter_t iter;
	for (iter=pairs.begin(); iter!=pairs.end(); iter++)
		if (iter->option.compare(option) == 0)
			return iter->value;

	/* it is assumed that the option is always found */
	assert(false);
	return std::string(""); // to avoid compiler errors
}