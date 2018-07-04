#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "common.h"

int TCPClient(const char* addr, const char* port)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo* result;
	int error = getaddrinfo(addr, port, &hints, &result);
	if (error)
	{
		printf("Error: %s\n", gai_strerror(error));
		return 0;
	}

	int sd;
	for (struct addrinfo* i = result; i != NULL; i = i->ai_next)
	{
        	sd = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
		if (sd == -1) continue;
		error = connect(sd, i->ai_addr, i->ai_addrlen);
		if (!error) break;
		close(sd);
	}
	freeaddrinfo(result);

	if (sd == -1 || error) return 0;

	return sd;
}

int Transfer(const char* filename, const char* server, const char* port)
{
	FILE* file = fopen(filename, "rb");
	if (!file)
	{
		printf("Error: Can't open %s\n", filename);
		return -1;
	}
	else printf("Open %s OK\n", filename);

	int sock = TCPClient(server, port);
	if (sock) printf("Connect OK\n");
	else
	{
		printf("Error: Can't connect to server\n");
		return -1;
	}

	int error = SendMessage(sock, filename);
	if (error)
	{
		printf("Error: Can't send filename to server\n");
		return -1;
	}

	Message* message = (Message*) ReadMessage(sock, NULL);
	if (!message)
	{
		printf("Error: Bad response from server\n");
		return -1;
	}

	error = strcmp(message->data, "OK");
	if (error)
	{
		printf("Error: %s\n", message->data);
		free(message);
		return -1;
	}

	char buffer[BUFFER_SIZE];
	do
	{
		size_t bytes = fread(buffer, sizeof(char), BUFFER_SIZE, file);
		if (bytes == BUFFER_SIZE)
		{
			ssize_t sent = send(sock, buffer, bytes, 0);
			if (sent == -1)
			{
				printf("Error: Can't write to socket\n");
				shutdown(sock, SHUT_RDWR);
				close(sock);
				fclose(file);
				return -1;
			}
		}
		else
		{
			if (ferror(file))
			{
				printf("Error: Can't read %s\n", filename);
				fclose(file);
				return 1;
			}
			else if (feof(file))
			{
				ssize_t sent = send(sock, buffer, bytes, 0);
				if (sent == -1)
				{
					printf("Error: Can't write to socket\n");
					shutdown(sock, SHUT_RDWR);
					close(sock);
					fclose(file);
					return -1;
				}
				break;
			}
		}
	} while (1);

	shutdown(sock, SHUT_RDWR);
	close(sock);
	fclose(file);
	return 0;
}

int main(int argc, char* argv[])
{
	if (!argv[1] || !argv[2] || !argv[3])
	{
		printf("Usage: client <file> <server> <port or service name>\n");
		return 0;
	}

	int error = Transfer(argv[1], argv[2], argv[3]);
	if (error) printf("Error: %s upload failed\n", argv[1]);
	else printf("%s uploaded\n", argv[1]);

	return 0;
}