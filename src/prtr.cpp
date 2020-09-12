#include "prtr.hpp"

#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
namespace pr
{
	namespace
	{
		struct ChromeTraceProcess
		{
			std::vector<ChromeTraceEvent> events;
			std::string process;
		};
		clock::time_point g_timeBase = clock::now();
		std::mutex g_mutex;
		std::map<std::thread::id, ChromeTraceProcess> g_processList;

		// https://stackoverflow.com/questions/7724448/simple-json-string-escape-for-c
		std::string escape_json( const std::string& s )
		{
			std::ostringstream o;
			for( auto c = s.cbegin(); c != s.cend(); c++ )
			{
				switch( *c )
				{
				case '"':
					o << "\\\"";
					break;
				case '\\':
					o << "\\\\";
					break;
				case '\b':
					o << "\\b";
					break;
				case '\f':
					o << "\\f";
					break;
				case '\n':
					o << "\\n";
					break;
				case '\r':
					o << "\\r";
					break;
				case '\t':
					o << "\\t";
					break;
				default:
					if( '\x00' <= *c && *c <= '\x1f' )
					{
						o << "\\u"
						  << std::hex << std::setw( 4 ) << std::setfill( '0' ) << (int)*c;
					}
					else
					{
						o << *c;
					}
				}
			}
			return o.str();
		}
	} // namespace
	ChromeTraceTimer::ChromeTraceTimer()
	{
		_event.started_at = clock::now();
	}
	ChromeTraceEvent ChromeTraceTimer::getElapsedEvent() const
	{
		auto e = _event;
		e.duration = clock::now() - e.started_at;
		return e;
	}
	void ChromeTraceTimer::label( const char* format, ... )
	{
		va_list ap;
		va_start( ap, format );
		char buffer[64];
		int bytes = vsnprintf( buffer, sizeof( buffer ), format, ap );
		va_end( ap );

		if( sizeof( buffer ) <= bytes )
		{
			va_list ap;
			va_start( ap, format );
			char* dyBuffer = (char*)malloc( bytes + 1 );
			vsnprintf( dyBuffer, bytes + 1, format, ap );
			va_end( ap );
			_event.name = dyBuffer;
			free( dyBuffer );
		}
		else
		{
			_event.name = buffer;
		}
	}
	std::string& ChromeTraceTimer::operator[]( std::string key )
	{
		return _event.metadata[key];
	}
	void ChromeTraceAddEvent( const ChromeTraceEvent& e )
	{
		auto ID = std::this_thread::get_id();
		std::lock_guard<std::mutex> scoped_lock( g_mutex );
		g_processList[ID].events.push_back( e );
	}
	void ChromeTraceSetProcessName( std::string process )
	{
		auto ID = std::this_thread::get_id();
		std::lock_guard<std::mutex> scoped_lock( g_mutex );
		g_processList[ID].process = process;
	}
	std::string ChromeTraceGetTrace()
	{
		std::lock_guard<std::mutex> scoped_lock( g_mutex );
		std::stringstream ss;
		ss << "{" << std::endl;
		ss << "    \"traceEvents\": [" << std::endl;

		for( auto it = g_processList.begin(); it != g_processList.end(); ++it )
		{
			for( const ChromeTraceEvent& e : it->second.events )
			{
				ss << "        ";
				ss << R"({ "pid" : ")";
				if( it->second.process.empty() )
				{
					ss << it->first;
				}
				else
				{
					ss << escape_json( it->second.process );
				}
				ss << R"(", "ts" : ")";
				ss << std::chrono::duration_cast<std::chrono::microseconds>( e.started_at - g_timeBase ).count();
				ss << R"(", "dur" : ")";
				ss << std::chrono::duration_cast<std::chrono::microseconds>( e.duration ).count();
				ss << R"(", "name" : ")";
				ss << escape_json( e.name );
				ss << R"(", "args" : { )";

				for( auto it = e.metadata.begin(); it != e.metadata.end(); ++it )
				{
					ss << R"(")" << escape_json( it->first ) << R"(" : ")" << escape_json( it->second ) << R"(")";
					if( std::next( it ) != e.metadata.end() )
					{
						ss << ", ";
					}
				}

				ss << R"(}, "ph" : "X" },)" << std::endl;
			}
		}
		ss << "        {}" << std::endl;

		ss << "    ]" << std::endl;
		ss << "}" << std::endl;
		return ss.str();
	}
	void ChromeTraceClear()
	{
		std::lock_guard<std::mutex> scoped_lock( g_mutex );
		g_processList.clear();
	}
} // namespace pr