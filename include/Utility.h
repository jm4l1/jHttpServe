#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

namespace Utility
{
	static void ltrim(std::string& s)
	{
		auto it = std::find_if(s.begin(),
							   s.end(),
							   [](char c)
							   {
								   return !std::isspace<char>(c, std::locale::classic());
							   });
		s.erase(s.begin(), it);
	}

	static void rtrim(std::string& s)
	{
		auto it = std::find_if(s.rbegin(),
							   s.rend(),
							   [](char c)
							   {
								   return !std::isspace<char>(c, std::locale::classic());
							   });
		s.erase(it.base(), s.end());
	}

	static void trim(std::string& s)
	{
		ltrim(s);
		rtrim(s);
	}

	static std::string GetDate()
	{
		auto now = std::chrono::system_clock::now();
		auto date = std::chrono::system_clock::to_time_t(now);
		std::stringstream date_stream;
		date_stream << std::put_time(std::gmtime(&date), "%a, %d %b %Y %OH:%M:%S GMT");
		return date_stream.str();
	};
	static std::string GetshortDate()
	{
		auto now = std::chrono::system_clock::now();
		auto date = std::chrono::system_clock::to_time_t(now);
		std::stringstream date_stream;
		date_stream << std::put_time(std::localtime(&date), "%Y%m%d_%OH:%M:%S");
		return date_stream.str();
	};

}  // namespace Utility
#endif