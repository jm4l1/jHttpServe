#ifndef __ARGS_PARSER_H_
#define __ARGS_PARSER_H_

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>


namespace fs = std::filesystem;
struct Option
{
	char flag;
	std::string arg;
	std::string description;
	bool is_required;
};

class ArgsParser
{
public:
	ArgsParser(const std::string& prog_name)
	  : _prog_name(prog_name)
	{
	}

	void Parse(int argc, char* argv[])
	{
		int count = 1;
		while (count < (argc - 1))
		{
			std::string arg = argv[count];
			std::regex flag_regex("^-(.)");
			std::smatch flag_match;
			if (!std::regex_match(arg, flag_match, flag_regex))
			{
				PrintUsage();
				exit(EXIT_FAILURE);
			};
			if (std::find(_allowed_flags.begin(), _allowed_flags.end(), flag_match[1]) == _allowed_flags.end())
			{
				std::cout << "Error : Invalid flag " << arg << "\n";
				PrintUsage();
				exit(EXIT_FAILURE);
			}
			if (flag_match.str(0)[1] == 'h')
			{
				PrintUsage();
				exit(EXIT_SUCCESS);
			}
			if (count + 1 > argc)
			{
				std::cout << "Error : Insufficient arguments provided!!\n\n";
				PrintUsage();
				exit(EXIT_FAILURE);
			}
			_argument_map.insert({ flag_match.str(0)[1], argv[count + 1] });
			count += 2;
		}
		for (auto flag : _required_flags)
		{
			if (_argument_map.find(flag) == _argument_map.end())
			{
				std::cout << "Error : Flag -" << flag << " is required\n\n";
				PrintUsage();
				exit(EXIT_FAILURE);
			}
		}
	}

	void AddOption(char flag, const std::string& flag_arg, const std::string& description, bool is_required)
	{
		_allowed_flags.emplace_back(flag);
		if (is_required)
		{
			_required_flags.emplace_back(flag);
		}
		Option option = { .flag = flag, .arg = flag_arg, .description = description, .is_required = is_required };
		options.emplace_back(option);
	}

	void PrintOptionsSummary()
	{
		std::cout << "Usage: ";
		std::cout << _prog_name;
		std::cout << " [-h]";
		std::for_each(options.begin(),
					  options.end(),
					  [](auto& option)
					  {
						  std::cout << " [-";
						  std::cout << option.flag << " ";
						  std::cout << option.arg << "]";
					  });
		std::cout << "\n";
	}

	void PrintOptionsDetails()
	{
		std::for_each(options.begin(),
					  options.end(),
					  [](auto& option)
					  {
						  std::cout << '\t' << option;
					  });
		std::cout << "\n";
	}

	void PrintUsage()
	{
		PrintOptionsSummary();
		std::cout << "\t-h : show help menu\n";
		PrintOptionsDetails();
	}

	std::unordered_map<char, std::string> GetArgs() const
	{
		return _argument_map;
	}

private:
	std::string _prog_name;
	std::unordered_map<char, std::string> _argument_map;
	std::vector<char> _allowed_flags;
	std::vector<char> _required_flags;
	std::vector<Option> options;
};
std::ostream& operator<<(std::ostream& os, const Option& obj)
{
	os << "-" << obj.flag << " " << obj.arg << ":"
	   << " " << obj.description << "\n";
	return os;
}
#endif