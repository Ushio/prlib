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
	ChromeTraceTimer::ChromeTraceTimer(AddMode mode):_mode(mode)
	{
		_event.started_at = clock::now();
	}
	ChromeTraceTimer::~ChromeTraceTimer()
	{
		if( _mode == AddMode::Auto )
		{
			ChromeTraceAddEvent( getElapsedEvent() );
		}
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
	void ChromeTraceSetProcessName( const char* format, ... )
	{
		std::string process;
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
			process = dyBuffer;
			free( dyBuffer );
		}
		else
		{
			process = buffer;
		}

		auto ID = std::this_thread::get_id();
		std::lock_guard<std::mutex> scoped_lock( g_mutex );
		g_processList[ID].process = process;
	}
	std::string ChromeTraceGetTrace()
	{
		std::lock_guard<std::mutex> scoped_lock( g_mutex );
		std::string ss;
		ss.append( "{\n" );
		ss.append("    \"traceEvents\": [\n");

		for( auto it = g_processList.begin(); it != g_processList.end(); ++it )
		{
			std::string pid;
			if (it->second.process.empty())
			{
				std::ostringstream s;
				s.imbue(std::locale("C"));
				s << it->first;
				pid = s.str();
			}
			else
			{
				pid = escape_json(it->second.process);
			}

			for( const ChromeTraceEvent& e : it->second.events )
			{
				ss.append("        ");
				ss.append(R"({ "pid" : ")");
				ss.append(pid);
				ss.append(R"(", "ts" : ")");
				ss.append(std::to_string(std::chrono::duration_cast<std::chrono::microseconds>( e.started_at - g_timeBase ).count()));
				ss.append(R"(", "dur" : ")");
				ss.append(std::to_string(std::chrono::duration_cast<std::chrono::microseconds>( e.duration ).count()));
				ss.append(R"(", "name" : ")");
				ss.append(escape_json( e.name ));
				ss.append(R"(", "args" : { )");

				for( auto it = e.metadata.begin(); it != e.metadata.end(); ++it )
				{
					ss.append( std::string(R"(")") + escape_json( it->first ) + R"(" : ")" + escape_json( it->second ) + R"(")");
					if( std::next( it ) != e.metadata.end() )
					{
						ss.append( ", ");
					}
				}

				ss.append(R"(}, "ph" : "X" },)"); ss.append("\n");
			}
		}
		ss.append("        {}\n");

		ss.append("    ]\n");
		ss.append("}\n");
		return ss;
	}
	void ChromeTraceClear()
	{
		std::lock_guard<std::mutex> scoped_lock( g_mutex );
		g_processList.clear();
	}
} // namespace pr