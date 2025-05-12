#ifndef HTTP_H
#define HTTP_H

#define WIN32_LEAN_AND_MEAN
#include <base_inc.h>
#include <base_inc.cpp>

#include <winsock2.h>
#include <ws2tcpip.h>


enum Http_Version
{
	Http_Invalid_Version,
	
	Http_0_9,
	Http_1_0,
	Http_1_1,
};

enum Http_Method
{
	Http_Method_Unknown = 0,
	
	Http_Get,
	Http_Post,
	Http_Put,
	Http_Patch,
	Http_Delete,
};

enum Http_StatusCode
{
#define HttpAction(a, b) Http_##a = b,
#include "HttpStatus.def"
#undef HttpAction 
};


struct URI
{
	String8 Path;

	#define DEFAULT_QUERY_CAP 8
	String8 QueryKeys[DEFAULT_QUERY_CAP];
	String8 QueryValues[DEFAULT_QUERY_CAP];
	u32 QueryCount;
};

struct Request
{
	Http_Version Version;
	Http_Method Method;
	URI URI;

	String8* Keys;
	String8* Values;
	u32 HeaderCount;
	
	String8 Body;
};

struct Response
{
	Http_Version Version;
	Http_StatusCode Status;

#define DEFAULT_HEADER_CAP 32
	String8 Keys[DEFAULT_HEADER_CAP];
	String8 Values[DEFAULT_HEADER_CAP];
	u32 HeaderCount;

	String8 Body;
};


typedef Response(*HandleFunc)(Arena*, Request*);

struct Route
{
	Http_Method Method;
	String8 Path;
	HandleFunc Func;	
};

struct Router
{
#define DEFAULT_ROUTE_CAP 128
	Route Routes[DEFAULT_ROUTE_CAP];
	u64 RouteCount;
};

struct HttpServer
{
	char* Host;
	char* Port;
	Router* Router;
};

#endif // Header guard
