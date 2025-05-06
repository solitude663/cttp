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
		LogError(0, "WSAStartup failed");
        exit(1);
    }

    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2)
    {
        LogError(0, "Version 2.2 of Winsock not available.");
        WSACleanup();
        exit(2);
    }
}

internal void MainEntry(i32 argc, char** argv)
{
	UnusedVariable(argc);
	UnusedVariable(argv);
	
	WinsockInit();

	char* host_name = "localhost";
	char* port = "3490";

	addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	addrinfo* server_info;	
	if(getaddrinfo(host_name, port, &hints, &server_info) != 0)
	{
		LogError(0, "Failed to get addrinfo.");
		exit(1);
	}

	SOCKET sock = socket(server_info->ai_family,
						 server_info->ai_socktype,
						 server_info->ai_protocol);
	if(sock == INVALID_SOCKET)
	{
		LogError(0, "Failed to create socket.");
		exit(1);
	}

	if(connect(sock, server_info->ai_addr, (int)server_info->ai_addrlen) == SOCKET_ERROR)
	{
		LogError(0, "Failed to bind socket.");
		exit(1);
	}


	printf("Connected to server\n");

	u8 recv_buffer[1024] = {0};
	u32 recv_buffer_len = 1024;

	char message[1024] = "Hello, Server";
	for(i32 idx = 0; idx < 10; idx++)
	{		
		send(sock, message, (int)CStringLength(message), 0);
		printf("[Client] Sent: %s\n", message);

		recv(sock, (char*)recv_buffer, recv_buffer_len, 0);
		printf("[Client] Recv: %.*s\n", recv_buffer_len, recv_buffer);

		printf("> ");
		fgets(message, sizeof(message) - 1, stdin);
		message[CStringLength(message) - 1] = 0;

		if(Str8Match(message, ".exit", MF_None)) break;		
	}
	
	closesocket(sock);
	
}

int main(int argc, char** argv)
{
	BaseMainThreadEntry(MainEntry, argc, argv);
	return 0;
}
