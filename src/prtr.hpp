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
	void ChromeTraceSetProcessName( const char* format, ... );
	std::string ChromeTraceGetTrace();
	void ChromeTraceClear();

	class ChromeTraceTimer
	{
	public:
		enum class AddMode
		{
			Auto,
			Manual
		};
		ChromeTraceTimer( AddMode mode );
		~ChromeTraceTimer();
		ChromeTraceTimer( const ChromeTraceTimer& ) = delete;
		ChromeTraceTimer& operator=( const ChromeTraceTimer& ) = delete;

		ChromeTraceEvent ChromeTraceTimer::getElapsedEvent() const;
		void label( const char* format, ... );
		std::string& operator[]( std::string key );

	private:
		AddMode _mode;
		ChromeTraceEvent _event;
	};
} // namespace pr
