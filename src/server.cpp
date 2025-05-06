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

internal void HandleConnection(SOCKET client_sock)
{
	TempArena temp = GetScratch(0);
	
	i32 counter = 0;
	u8 recv_buffer[1024] = {0};
	u32 recv_buffer_len = 1024;
	for(;;)
	{
		i32 bytes_recieved = recv(client_sock, (char*)recv_buffer, recv_buffer_len, 0);
		
		if(bytes_recieved > 0)
		{
			printf("[SERVER] Recv: %.*s\n", bytes_recieved, (char*)recv_buffer);
			
			String8 message = Str8Format(temp.Arena, "Counter: %d", counter++);
			send(client_sock, (char*)message.Str, (i32)message.Length, 0);
			printf("[SERVER] Sent: %.*s\n", Str8Print(message));
		}
		else if(bytes_recieved == 0)
		{
			printf("[SERVER] Client closed connection...\n");
			break;
		}
		else
		{
			LogPanic(0, "Recv failed.");
		}		
	}

	ReleaseScratch(temp);
}

internal void MainEntry(i32 argc, char** argv)
{
	UnusedVariable(argc);
	UnusedVariable(argv);

	WinsockInit();

	// Arena* arena = ArenaAllocDefault();
	
	char* host_name = 0; // "www.archlinux.org";
	char* port = "23";

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
		HandleConnection(client_sock);
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
