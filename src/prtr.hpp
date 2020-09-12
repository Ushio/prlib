#pragma once

#include <chrono>
#include <map>
#include <stdarg.h>
#include <string>

namespace pr
{
	using clock = std::chrono::steady_clock;
	struct ChromeTraceEvent
	{
		std::string name;
		std::map<std::string, std::string> metadata;
		clock::time_point started_at;
		clock::duration duration;
	};

	void ChromeTraceAddEvent( const ChromeTraceEvent& e );
	void ChromeTraceSetProcessName( std::string process );
	std::string ChromeTraceGetTrace();
	void ChromeTraceClear();

	class ChromeTraceTimer
	{
	public:
		ChromeTraceTimer();
		ChromeTraceEvent ChromeTraceTimer::getElapsedEvent() const;
		void label( const char* format, ... );
		std::string& operator[]( std::string key );

	private:
		ChromeTraceEvent _event;
	};
} // namespace pr
