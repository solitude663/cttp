#define WIN32_LEAN_AND_MEAN
#include <base_inc.h>
#include <base_inc.cpp>

#include <winsock2.h>
#include <ws2tcpip.h>

internal void WinsockInit()
{
	WSADATA wsa_data;	
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
	{
		LogPanic(0, "WSAStartup failed");
    }

    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2)
    {
        LogPanic(0, "Version 2.2 of Winsock not available.");
        WSACleanup();
    }
}

internal String8 PrintIP(Arena* arena, SOCKADDR_STORAGE addr)
{
	char ip_buffer[INET6_ADDRSTRLEN] = {0};
	if(addr.ss_family == AF_INET)
	{
		sockaddr_in* addr = (sockaddr_in*)&addr;
		inet_ntop(AF_INET, &addr->sin_addr, ip_buffer, INET6_ADDRSTRLEN);
	}
	else
	{
		sockaddr_in6* addr = (sockaddr_in6*)&addr;
		inet_ntop(AF_INET, &addr->sin6_addr, ip_buffer, INET6_ADDRSTRLEN);
	}

	return Str8Copy(arena, Str8C(ip_buffer));
}

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
#define HttpAction(a, b) Http_ ## a = b,
#include "HttpStatus.def"
#undef HttpAction 
};

// NOTE(afb) :: Doesn't work in C++ but works in C because C++ is a shit fucking
// language. Tempting reason to use C.
// char* HttpStatusStr[] = {
// #define HttpAction(a, b) [b] = #a,
// #include "HttpStatus.def"
// #undef HttpAction
// };

struct Request
{
	Http_Version Version;
	Http_Method Method;
	String8 URI;

	String8* Keys;
	String8* Values;
	u32 HeaderCount;
	
	String8 Body;
};

struct Response
{
	Http_Version Version;
	Http_StatusCode Status;

	String8* Keys;
	String8* Values;
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
#define DEFAULT_ROUTE_CAP 1024
	Route Routes[DEFAULT_ROUTE_CAP];
	u64 RouteCount;
};



internal Http_Method GetHttpMethod(String8 method)
{
	Http_Method result = Http_Method_Unknown;
	if(Str8Match(method, "GET", MF_None)) result = Http_Get;
	else if(Str8Match(method, "POST", MF_None)) result = Http_Post;
	else if(Str8Match(method, "PUT", MF_None)) result = Http_Put;
	else if(Str8Match(method, "PATCH", MF_None)) result = Http_Patch;
	else if(Str8Match(method, "DELETE", MF_None)) result = Http_Delete;

	return result;
}

internal Http_Version GetHttpVersion(String8 version)
{
	Http_Version result = Http_Invalid_Version;
	if(Str8Match(version, "HTTP/1.0", MF_None)) result = Http_1_0;
	if(Str8Match(version, "HTTP/1.1", MF_None)) result = Http_1_1;

	return result;
}

internal Request ParseRequest(Arena* arena, String8 req)
{
	Request result = {0};

	String8 header;
	u64 index = Str8Find(req, "\r\n\r\n");
	header = Prefix8(req, index);
	result.Body = Substr8(req, index + 4, req.Length - index);

	Assert(index != req.Length); // Malformed request
	
	String8List header_lines = Str8Split(arena, header, "\r\n");
	String8List request_line = Str8Split(arena, header_lines.First->Str, " ");
	Assert(request_line.NodeCount == 3);

	String8 method = request_line.First->Str;
	String8 uri = request_line.First->Next->Str;
	String8 version = request_line.Last->Str;	

	result.Method = GetHttpMethod(method);
	result.URI = uri;
	result.Version = GetHttpVersion(version);

	u32 header_line_count = (u32)header_lines.NodeCount - 1;
	String8* keys = PushArray(arena, String8, header_line_count);
	String8* values = PushArray(arena, String8, header_line_count);

	String8Node* node = header_lines.First->Next;
	for(u32 idx = 0; idx < header_line_count; idx++)
	{		
		// TODO(afb) :: Hacky. Find a fix
		String8List line = Str8Split(arena, node->Str, ": "); 
		Assert(line.NodeCount == 2);
		
		String8 key = line.First->Str;
		String8 value = Trim8Space(line.Last->Str);

		keys[idx] = key;
		values[idx] = value;
		node = node->Next;
	}

	result.Keys = keys;
	result.Values = values;
	result.HeaderCount = header_line_count;

	return result;
}

HandleFunc GetHandler(Router* router, Request* req)
{
	HandleFunc result = 0;
	
	for(u64 i = 0; i < router->RouteCount; i++)
	{
		if(router->Routes[i].Method == req->Method &&
		   router->Routes[i].Path == req->URI)
		{
			result = router->Routes[i].Func;
			break;
		}
	}
	return result;
}

internal void HandleConnection(Arena* arena, Router* router, SOCKET client_sock)
{
	u8 recv_buffer[1024] = {0};
	u32 recv_buffer_len = 1024;
	
	for(;;)
	{
		i32 bytes_recieved = recv(client_sock, (char*)recv_buffer, recv_buffer_len, 0);
		if(bytes_recieved > 0)
		{
			printf("[SERVER] Recv: %d bytes\n", bytes_recieved);
			Request req = ParseRequest(arena, String8(recv_buffer, bytes_recieved));
			
			if((req.Version == Http_Invalid_Version) ||
			   (req.Method == Http_Method_Unknown))
			{
				String8 message =
					"HTTP/1.1 400 Bad Request\r\n"
					"Content-Length: 0\r\n"
					"Connection: close\r\n"
					"\r\n";
				LogError(0, "Bad request");
				send(client_sock, (char*)message.Str, (i32)message.Length, 0);
				break;
			}

			HandleFunc func = GetHandler(router, &req);
			if(!func)
			{
				String8 message =
					"HTTP/1.1 404 Not Found\r\n"
					"Content-Length: 0\r\n"
					"Connection: close\r\n"
					"\r\n";
				LogError(0, "Not found");
				send(client_sock, (char*)message.Str, (i32)message.Length, 0);
				break;
			}

			Response res = func(arena, &req);				
			String8 response = {0};
			
			i32 bytes_sent = send(client_sock, (char*)response.Str,
								  (i32)response.Length, 0);
			if(bytes_sent == SOCKET_ERROR)
			{
				LogError(0, "Send Error");
				break;
			}
			printf("[SERVER] Sent: %lld bytes\n", response.Length);
		}
		else if(bytes_recieved == 0)
		{
			printf("[SERVER] Client closed connection...\n");
			break;
		}
		else
		{
			LogError(0, "Recv failed.");
			break;
		}		
	}

	// ReleaseScratch(temp);
}

internal void RegisterRoute(Router* router, Http_Method method,
							String8 path, HandleFunc func)
{
	Route route = {0};
	route.Path = path;
	route.Func = func;
	route.Method = method;
	router->Routes[router->RouteCount++] = route;
}

internal Response Temp(Arena* arena, Request* req)
{
	Response result = {0};

	// printf("FOUND %.*s\n", Str8Print(req->URI));
	UnusedVariable(arena);
	UnusedVariable(req);
	
	return result;
}

internal void MainEntry(i32 argc, char** argv)
{
	UnusedVariable(argc);
	UnusedVariable(argv);

	WinsockInit();

	// Arena* arena = ArenaAllocDefault();

	Router router = {(Http_Method)0};
	RegisterRoute(&router, Http_Get, "/index", Temp);
	RegisterRoute(&router, Http_Post, "/login", Temp);
	RegisterRoute(&router, Http_Post, "/shop/checkout", Temp);

	
	char* host_name = 0; // "www.archlinux.org";
	char* port = "8080";

	addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	
	addrinfo* server_info = 0;	
	if(getaddrinfo(host_name, port, &hints, &server_info) != 0)
	{
		WSACleanup();
		LogPanic(0, "Failed to get addrinfo.");
	}
	
	SOCKET server_socket = socket(server_info->ai_family,
								  server_info->ai_socktype,
								  server_info->ai_protocol);
	if(server_socket == INVALID_SOCKET)
	{
		WSACleanup();
		LogPanic(0, "Failed to create socket.");
	}

	{
		BOOL yes = TRUE;
		if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) == SOCKET_ERROR)
		{
			LogPanic(0, "Failed to setsocketopt .");
		}
	}
	
	if(bind(server_socket, server_info->ai_addr, (int)server_info->ai_addrlen) == SOCKET_ERROR)
	{
		WSACleanup();
		LogPanic(0, "Failed to bind socket.");
	}	
	
	if(listen(server_socket, SOMAXCONN) == SOCKET_ERROR)
	{
		WSACleanup();
		LogPanic(0, "Failed to listen on socket.");
	}

	printf("Listening on port %s\n", port);
	for(;;)
	{
		TempArena temp = GetScratch(0);

		SOCKADDR_STORAGE addr_storage = {0};
		i32 addr_storage_len = sizeof(addr_storage);
		SOCKET client_sock = accept(server_socket, (sockaddr*)&addr_storage, &addr_storage_len);
		if(client_sock == INVALID_SOCKET)
		{
			WSACleanup();
			LogError(0, "Failed accept. Closing server");
			break;
		}
		
		String8 ip = PrintIP(temp.Arena, addr_storage);
		printf("[SERVER] Connected to %.*s\n", Str8Print(ip));
		HandleConnection(temp.Arena, &router, client_sock);
		printf("[SERVER] Connection to %.*s closed\n", Str8Print(ip));
		closesocket(client_sock);		
		ReleaseScratch(temp);
	}	
	
	closesocket(server_socket);
	WSACleanup();
}

int main(int argc, char** argv)
{
	BaseMainThreadEntry(MainEntry, argc, argv);
	return 0;
}
