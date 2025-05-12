
#include "http.h"
#include "http.cpp"


internal Response Temp(Arena* arena, Request* req)
{
	Response result = {0};

	String8 name = req->Body;	
	result.Body = Str8Format(arena, "Hello %S!!", name);
	result.Status = Http_StatusOK;	
	return result;
}

internal void HttpServe(HttpServer* server)
{
	addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	
	addrinfo* server_info = 0;	
	if(getaddrinfo(server->Host, server->Port, &hints, &server_info) != 0)
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
		HandleConnection(temp.Arena, server->Router, client_sock);
		printf("[SERVER] Connection to %.*s closed\n", Str8Print(ip));
		closesocket(client_sock);		
		ReleaseScratch(temp);
	}	
	
	closesocket(server_socket);
	WSACleanup();
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

	HttpServer server = {0};
	server.Router = &router;
	server.Host = 0;
	server.Port = "8080";

	printf("Listening on port %s\n", server.Port);
	HttpServe(&server);
	
}

int main(int argc, char** argv)
{
	BaseMainThreadEntry(MainEntry, argc, argv);
	return 0;
}
