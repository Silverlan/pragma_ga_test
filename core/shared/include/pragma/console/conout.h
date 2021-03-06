/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#ifndef __CONOUT_H__
#define __CONOUT_H__
#include "pragma/definitions.h"
#include "pragma/console/util_console_color.hpp"
#include <iostream>
#include <sstream>
#include <functional>
#include <string_view>
#ifdef _WIN32
	#include <Windows.h>
#else
	#define DWORD unsigned int
	#define FOREGROUND_BLUE 1
	#define FOREGROUND_GREEN 2
	#define FOREGROUND_RED 4
	#define FOREGROUND_INTENSITY 8
	#define BACKGROUND_BLUE 16
	#define BACKGROUND_GREEN 32
	#define BACKGROUND_RED 64
	#define BACKGROUND_INTENSITY 128
#endif

struct Color;
namespace Con {class c_crit;};
template <class T> Con::c_crit& operator<< (Con::c_crit &con,const T &t);
namespace Con
{
	class DLLNETWORK c_cout {};
	class DLLNETWORK c_cwar {};
	class DLLNETWORK c_cerr {};
	class DLLNETWORK c_crit
	{
	private:
		std::stringstream m_message;
		bool m_bActivated = false;
	public:
		friend DLLNETWORK std::basic_ostream<char,std::char_traits<char>> &endl(std::basic_ostream<char,std::char_traits<char>>& os);
		template <class T>
			friend Con::c_crit& ::operator<<(Con::c_crit &con,const T &t);
	};
	class DLLNETWORK c_csv {};
	class DLLNETWORK c_ccl {};
	extern DLLNETWORK c_cout cout;
	extern DLLNETWORK c_cwar cwar;
	extern DLLNETWORK c_cerr cerr;
	extern DLLNETWORK c_crit crit;
	extern DLLNETWORK c_csv csv;
	extern DLLNETWORK c_ccl ccl;
	DLLNETWORK std::basic_ostream<char,std::char_traits<char>> &endl(std::basic_ostream<char,std::char_traits<char>>& os);
	DLLNETWORK void flush();
	DLLNETWORK void attr(DWORD attr);
	DLLNETWORK void WriteToLog(std::stringstream &ss);
	DLLNETWORK void WriteToLog(std::string str);
	DLLNETWORK int GetLogLevel();

	enum class MessageFlags : uint8_t
	{
		None = 0u,
		Generic = 1u,
		Warning = Generic<<1u,
		Error = Warning<<1u,
		Critical = Error<<1u,

		ServerSide = Critical<<1u,
		ClientSide = ServerSide<<1u
	};
	DLLNETWORK void set_output_callback(const std::function<void(const std::string_view&,MessageFlags,const ::Color*)> &callback);
	DLLNETWORK const std::function<void(const std::string_view&,MessageFlags,const ::Color*)> &get_output_callback();
	DLLNETWORK void print(const std::string_view &sv,const ::Color &color,MessageFlags flags=MessageFlags::None);
	DLLNETWORK void print(const std::string_view &sv,MessageFlags flags=MessageFlags::None);
	template<typename T>
		inline void invoke_output_callback(const T &value,MessageFlags flags)
	{
		auto &outputCallback = Con::get_output_callback();
		if(outputCallback == nullptr)
			return;
		auto color = util::console_color_flags_to_color(util::get_active_console_color_flags());
		std::stringstream ss;
		ss<<value;
		outputCallback(ss.str(),flags,color.has_value() ? &(*color) : nullptr);
	}
};
REGISTER_BASIC_BITWISE_OPERATORS(Con::MessageFlags)

// c_cout
template <class T> Con::c_cout& operator<<(Con::c_cout &con,const T &t)
{
	std::cout<<t;
	if(Con::GetLogLevel() >= 3)
	{
		std::stringstream ss;
		ss<<t;
		Con::WriteToLog(ss);
	}
	invoke_output_callback(t,Con::MessageFlags::Generic);
	return con;
}
typedef std::ostream& (*conmanipulator) (std::ostream&);
DLLNETWORK Con::c_cout& operator<<(Con::c_cout& con,conmanipulator manipulator);
//

// c_cwar
template <class T> Con::c_cwar& operator<< (Con::c_cwar &con,const T &t)
{
	util::set_console_color(util::ConsoleColorFlags::Yellow | util::ConsoleColorFlags::Intensity);
	std::cout<<t;
	if(Con::GetLogLevel() >= 2)
	{
		std::stringstream ss;
		ss<<t;
		Con::WriteToLog(ss);
	}
	invoke_output_callback(t,Con::MessageFlags::Warning);
	return con;
}
typedef std::ostream& (*conmanipulator) (std::ostream&);
DLLNETWORK Con::c_cwar& operator<<(Con::c_cwar &con,conmanipulator manipulator);
//

// c_cerr
template <class T> Con::c_cerr& operator<< (Con::c_cerr &con,const T &t)
{
	util::set_console_color(util::ConsoleColorFlags::Red | util::ConsoleColorFlags::Intensity);
	std::cout<<t;
	if(Con::GetLogLevel() >= 1)
	{
		std::stringstream ss;
		ss<<t;
		Con::WriteToLog(ss);
	}
	invoke_output_callback(t,Con::MessageFlags::Error);
	return con;
}
typedef std::ostream& (*conmanipulator) (std::ostream&);
DLLNETWORK Con::c_cerr& operator<<(Con::c_cerr &con,conmanipulator manipulator);
//

// c_crit
template <class T> Con::c_crit& operator<< (Con::c_crit &con,const T &t)
{
	util::set_console_color(util::ConsoleColorFlags::BackgroundRed | util::ConsoleColorFlags::BackgroundIntensity | util::ConsoleColorFlags::Intensity | util::ConsoleColorFlags::White);
	std::stringstream ss;
	ss<<t;
	if(Con::GetLogLevel() >= 1)
		Con::WriteToLog(ss);
	invoke_output_callback(t,Con::MessageFlags::Critical);
	Con::crit.m_bActivated = true;
	Con::crit.m_message<<t;
	return con;
}
typedef std::ostream& (*conmanipulator) (std::ostream&);
DLLNETWORK Con::c_crit& operator<<(Con::c_crit &con,conmanipulator manipulator);
//

// c_csv
template <class T> Con::c_csv& operator<< (Con::c_csv &con,const T &t)
{
	util::set_console_color(util::ConsoleColorFlags::Cyan | util::ConsoleColorFlags::Intensity);
	std::cout<<t;
	if(Con::GetLogLevel() >= 2)
	{
		std::stringstream ss;
		ss<<t;
		Con::WriteToLog(ss);
	}
	invoke_output_callback(t,Con::MessageFlags::ServerSide);
	return con;
}
typedef std::ostream& (*conmanipulator) (std::ostream&);
DLLNETWORK Con::c_csv& operator<<(Con::c_csv &con,conmanipulator manipulator);
//

// c_ccl
template <class T> Con::c_ccl& operator<< (Con::c_ccl &con,const T &t)
{
	util::set_console_color(util::ConsoleColorFlags::Magenta | util::ConsoleColorFlags::Intensity);
	std::cout<<t;
	if(Con::GetLogLevel() >= 2)
	{
		std::stringstream ss;
		ss<<t;
		Con::WriteToLog(ss);
	}
	invoke_output_callback(t,Con::MessageFlags::ClientSide);
	return con;
}
typedef std::ostream& (*conmanipulator) (std::ostream&);
DLLNETWORK Con::c_ccl& operator<<(Con::c_ccl &con,conmanipulator manipulator);
//

#endif
