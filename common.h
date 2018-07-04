#ifndef __COMMON__
#define __COMMON__

#define BUFFER_SIZE (4096)
#define MESSAGE_MAX_SIZE (1024)
#define BACK_LOG_QUEUE_LENGTH (1000)

typedef struct Message_tag
{
	size_t length;
	char data[];
} Message;

typedef struct UploadQueue_tag
{
	int sock;
	FILE* file;
	struct UploadQueue_tag* next;
} UploadQueue;

char* ReadMessage(int sock, ssize_t* bytes);
int SendMessage(int sock, const char* message);
#endif