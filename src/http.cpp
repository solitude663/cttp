#include "http.h"

// NOTE(afb) :: Doesn't work in C++ but works in C because C++ is a shit fucking
// language. Tempting reason to use C.
// char* HttpStatusStr[] = {
// #define HttpAction(a, b) [b] = #a,
// #include "HttpStatus.def"
// #undef HttpAction
// };


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

internal URI ParseURI(String8 uri_str)
{
	URI result = {0};

	u64 pos = Str8Find(uri_str, "?");
	result.Path = Prefix8(uri_str, pos);

	if(pos < uri_str.Length)
	{
		u32 idx = 0;
		String8 query_section = Substr8(uri_str, pos+1, uri_str.Length - (pos+1));
		for(;;)
		{
			pos = Str8Find(query_section, "=");
			Assert(pos != query_section.Length);

			String8 key = Prefix8(query_section, pos);
			u64 end = Str8Find(query_section, "&");

			u64 start = pos + 1;
			u64 count = end - start;
			String8 value = Substr8(query_section, start, count);

			result.QueryKeys[idx] = key;
			result.QueryValues[idx++] = value;
			result.QueryCount++;
				
			if(end == query_section.Length) break;				
			query_section = Suffix8(query_section, query_section.Length - (end + 1));
		}		
	}
	
	return result;
}

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

internal String8 GetHttpVersionString(Arena* arena, Http_Version version)
{
	if(version == Http_1_0) return Str8Copy(arena, "HTTP/1.0");
	if(version == Http_1_1) return Str8Copy(arena, "HTTP/1.1");

	Assert(0);
	return EmptyString;
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
	URI uri = ParseURI(request_line.First->Next->Str);
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
		u64 pos = Str8Find(node->Str, ":");
		Assert(pos != node->Str.Length);
		
		String8 key = Trim8Space(Prefix8(node->Str, pos));
		String8 value = Trim8Space(Substr8(node->Str, pos+1, node->Str.Length - pos));

		keys[idx] = key;
		values[idx] = value;
		node = node->Next;
	}

	result.Keys = keys;
	result.Values = values;
	result.HeaderCount = header_line_count;

	return result;
}

internal String8 BuildResponse(Arena* arena, Response* res)
{
	String8 result = EmptyString;
	
	String8List sb = {0};

	// Status line
	{
		String8List status_builder = {0};		
		Str8ListPush(arena, &status_builder, GetHttpVersionString(arena, res->Version));
		Str8ListPushF(arena, &status_builder, "%d Temp Reason", (i64)res->Status);
		Str8ListPush(arena, &sb, Str8Join(arena, status_builder, " "));
	}

	for(u32 idx = 0; idx < res->HeaderCount; idx++)
	{
		Str8ListPushF(arena, &sb, "%S: %S", res->Keys[idx], res->Values);
	}

	Str8ListPushF(arena, &sb, "Content-Length: %d", res->Body.Length);
	Str8ListPush(arena, &sb, "Connection: close");

	String8 header = Str8Join(arena, sb, "\r\n");

	String8List builder = {0};
	Str8ListPush(arena, &builder, header);
	Str8ListPush(arena, &builder, res->Body);
	result = Str8Join(arena, builder, "\r\n\r\n");
	return result;
}

HandleFunc GetHandler(Router* router, Request* req)
{
	HandleFunc result = 0;
	for(u64 i = 0; i < router->RouteCount; i++)
	{
		if(router->Routes[i].Method == req->Method &&
		   req->URI.Path == router->Routes[i].Path)
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
			res.Version = req.Version;
			String8 response = BuildResponse(arena, &res);
			
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

internal void AddHeader(Response* res, String8 key, String8 value)
{
	Assert(res->HeaderCount < (DEFAULT_HEADER_CAP - 1));
	u32 idx = res->HeaderCount++;
	res->Keys[idx] = key;
	res->Values[idx] = value;
}
