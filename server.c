#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "common.h"

unsigned int UploadQueueLength = 0;

pthread_mutex_t mutex;
pthread_cond_t condition;

volatile sig_atomic_t ServerUp = 1;

UploadQueue* Queue = NULL;
 
void Stop(int sig)
{
	ServerUp = 0;
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&condition);
	pthread_mutex_unlock(&mutex);
}

UploadQueue* DeleteItem(UploadQueue* item)
{
	UploadQueue* tmp = Queue;
	--UploadQueueLength;
	if (item == Queue)
	{
		Queue = Queue->next;
		free(item);
		return Queue;
	}

	while (tmp->next != item) tmp = tmp->next;
	tmp->next = item->next;
	free(item);
	return tmp->next;
}

UploadQueue* ProcessBlock(UploadQueue* item)
{
	char buffer[BUFFER_SIZE];

	ssize_t bytes = recv(item->sock, buffer, BUFFER_SIZE, 0);
	if (bytes == -1 && errno == EAGAIN) return item->next;
	if (bytes > 0) fwrite(buffer, sizeof(char), bytes, item->file);
	else
	{
		shutdown(item->sock, SHUT_RDWR);
		close(item->sock);
		fclose(item->file);
		return DeleteItem(item);
	}
	return item->next;
}

void* threadFunc(void* thread_data)
{
	UploadQueue* tmp = Queue;

	while (ServerUp)
	{
		pthread_mutex_lock(&mutex);

		while (UploadQueueLength < 1 && ServerUp)
		{
			pthread_cond_wait(&condition, &mutex);
		}

		if (!tmp)
		{
			tmp = Queue;
			pthread_mutex_unlock(&mutex);
			continue;
		}

		tmp = ProcessBlock(tmp);
		
		pthread_mutex_unlock(&mutex);
	}

	pthread_exit(0);
}
 
int TCPServer(unsigned short port)
{
	int sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sd == -1)
	{
		printf("Error: Can't create socket\n");
		return 0;
	}

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_port = port;
	sa.sin_addr.s_addr = INADDR_ANY;

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100;
	setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(struct timeval));

	int error = bind(sd, (struct sockaddr*) &sa, sizeof(sa));
	if (error)
	{
		printf("Error: Can't bind socket\n");
		shutdown(sd, SHUT_RDWR);
		close(sd);
		return 0;
	}

	error = listen(sd, BACK_LOG_QUEUE_LENGTH);
	if (error)
	{
		printf("Error: Can't start server socket listen\n");
		shutdown(sd, SHUT_RDWR);
		close(sd);
		return 0;
	}

	return sd;
}

char* GetFilename(int sock)
{
	Message* message = (Message*) ReadMessage(sock, NULL);

	if (!message) return NULL;

	if (message->length != strlen(message->data))
	{
		free(message);
		return NULL;
	}

	char* filename = malloc(strlen(message->data) + 1);
	strcpy(filename, message->data);
	free(message);
	
	return filename;
}

int Exist(const char* filename)
{
	return !access(filename, F_OK);
}

int AddFile(const int sock, const char* filename)
{
	if (!filename) return -1;

	UploadQueue* tmp = (UploadQueue*) malloc(sizeof(UploadQueue));
	if (!tmp) return -1;
	tmp->sock = sock;
	tmp->file = fopen(filename, "w");
	if (!tmp->file)
	{
		free(tmp);
		return -1;
	}

	pthread_mutex_lock(&mutex);

	++UploadQueueLength;
	tmp->next = Queue;
	Queue = tmp;
	if (UploadQueueLength > 0) pthread_cond_signal(&condition);

	pthread_mutex_unlock(&mutex);

	return 0;
}

int main(int argc, char* argv[])
{
	signal(SIGINT, Stop);
	signal(SIGTSTP, Stop);

	if (!argv[1])
	{
		printf("Usage: server <port>\n");
		return 0;
	}

	void* thread_data = NULL;
	pthread_t thread;
	pthread_create(&thread, NULL, threadFunc, thread_data);

	unsigned short port;
	if (sscanf(argv[1], "%hu", &port) != 1)
	{
		printf("Error: Invalid port value\n");
		return 0;
	}
	if (port != atoi(argv[1]))
	{
		printf("Error: Invalid port value\n");
		return 0;
	}

	port = htons(port);
	int listener = TCPServer(port);
	if (listener) printf("Server is running, press [Ctrl + C] or [Ctrl + Z] for break\n");
	else
	{
		printf("Server error\n");
		return 0;
	}

	int sock = -1;
	int error = 0;
	while (ServerUp)
	{
		int sock = accept(listener, NULL, NULL);
		if (sock != -1)
		{
			char* filename = GetFilename(sock);
			if (filename)
			{
				if (Exist(filename))
				{
					SendMessage(sock, "File exist");
					shutdown(sock, SHUT_RDWR);
					close(sock);
					continue;
				}

				SendMessage(sock, "OK");
				error = AddFile(sock, filename);
				if (!error)
				{
					printf("Accepted %s\n", filename);
				}
				else
				{
					SendMessage(sock, "Can't process file");
					shutdown(sock, SHUT_RDWR);
					close(sock);
				}

				free(filename);
			}
		}
	}

	pthread_join(thread, NULL);

	shutdown(listener, SHUT_RDWR);
	close(listener);

	printf("Server stopped\n");

	return 0;
}