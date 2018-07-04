#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "common.h"

char* ReadMessage(int sock, ssize_t* bytes)
{
	ssize_t dummy;
	if (!bytes) bytes = &dummy;

	*bytes = 0;
	char buffer[BUFFER_SIZE];
	char* message = NULL;
	char* tmp = NULL;
	size_t size = 0;

	do
	{
		*bytes = recv(sock, buffer, BUFFER_SIZE, 0);
		if (*bytes == -1 && errno == EAGAIN) continue;
		if (*bytes == -1)
		{
			printf("Error: Can't read from socket\n");
			if (message) free(message);
			return NULL;
		}

		if (*bytes > 0)
		{
			tmp = message;
			message = (char*) malloc(size + *bytes);
			if (tmp)
			{
				memcpy(message, tmp, size);
				free(tmp);
			}

			memcpy(message + size, buffer, *bytes);
			size += *bytes;

			if (size > MESSAGE_MAX_SIZE)
			{
				printf("Error: Message is too big\n");
				if (message) free(message);
				return NULL;
			}

			if (*bytes < BUFFER_SIZE) break;
		}
	} while (*bytes > 0);

	if (!message) return NULL;

	return message;
}

int SendMessage(int sock, const char* data)
{
	size_t size = sizeof(size_t) + strlen(data) + 1;
	Message* message = (Message*) malloc(size);
	if (!message) return -1;

	message->length = strlen(data);
	strcpy(message->data, data);
	
	ssize_t sent = send(sock, message, size, 0);
	free(message);
	if (sent == -1)	return -1;
	return 0;
}