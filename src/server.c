/**
 * webserver.c -- A webserver written in C
 * 
 * Test with curl (if you don't have it, install it):
 * 
 *    curl -D - http://localhost:3490/
 *    curl -D - http://localhost:3490/d20
 *    curl -D - http://localhost:3490/date
 * 
 * You can also test the above URLs in your browser! They should work!
 * 
 * Posting Data:
 * 
 *    curl -D - -X POST -H 'Content-Type: text/plain' -d 'Hello, sample data!' http://localhost:3490/save
 * 
 * (Posting data is harder to test from a browser.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include <pthread.h>
#include "net.h"
#include "file.h"
#include "mime.h"
#include "cache.h"
#include "debug.c"

#define PORT "3490"  // the port users will be connecting to

pthread_mutex_t cachemutex;

struct thread_data {
	int sockfd;
	struct cache *cache;
};

#define SERVER_FILES "./serverfiles"
#define SERVER_ROOT "./serverroot"

/**
 * Send an HTTP response
 *
 * header:       "HTTP/1.1 404 NOT FOUND" or "HTTP/1.1 200 OK", etc.
 * content_type: "text/plain", etc.
 * body:         the data to send.
 * 
 * Return the value from the send() function.
 */
int send_response(int fd, char *header, char *content_type, void *body, int content_length)
{
	const int max_response_size = 262144 + content_length;
	char response[max_response_size];
	char buf[1024];

	memset(response, 0, max_response_size);
	memset(buf, 0, 1024);

	strcat(response, header);
	strcat(response, "\n");

	strcat(response, "Connection: Close\n");

	sprintf(buf, "Content-Type: %s\n", content_type);
	strcat(response, buf);

	sprintf(buf, "Content-Length: %d\n", content_length);
	strcat(response, buf);

	sprintf(buf, "Cache-Control: no-store\n");
	strcat(response, buf);

	strcat(response, "\n");

	printf("%s", response);

	int response_length = strlen(response);

	char *arr = (char *) body;
	for (int i = response_length, j = 0; j < content_length; j++, i++) {
		response[i] = arr[j];
	}

	response_length += content_length;

	// Send it all!
	int rv = send(fd, response, response_length, 0);

	if (rv < 0) {
		perror("send");
	}

	return rv;
}

/**
 * Send a /d20 endpoint response
 */
void get_d20(int fd)
{
	// Generate a random number between 1 and 20 inclusive
	srand(time(NULL));
	int rnum = (rand() % 20) + 1;

	// Use send_response() to send it back as text/plain data
	char rnumstr[32];
	sprintf(rnumstr, "%d", rnum);

	send_response(fd, "HTTP/1.1 200 OK", "text/plain", rnumstr, strlen(rnumstr));
}

/**
 * Send a 404 response
 */
void resp_404(int fd)
{
	char filepath[4096];
	struct file_data *filedata; 
	char *mime_type;

	// Fetch the 404.html file
	snprintf(filepath, sizeof(filepath), "%s/404.html", SERVER_FILES);
	filedata = file_load(filepath);

	if (filedata == NULL) {
		char *response404 = "404 FILE NOT FOUND";
		send_response(fd, "HTTP/1.1 404 NOT FOUND", "text/plain", response404, strlen(response404));
		return;
	}

	mime_type = mime_type_get(filepath);

	send_response(fd, "HTTP/1.1 404 NOT FOUND", mime_type, filedata->data, filedata->size);

	file_free(filedata);
}

struct myfile_data {
	char *buf;
	long int bufsize;
};

struct myfile_data *load_file(char *filename)
{
	long int bufsize;
	unsigned char *buf;

	FILE *f = fopen(filename, "r");

	if (f == NULL) {
		return NULL;
	}

	fseek(f, 0L, SEEK_END);
	bufsize = ftell(f);
	fseek(f, 0L, SEEK_SET);

	buf = malloc(bufsize * sizeof(char));
	fread(buf, 1, bufsize, f);

	fclose(f);

	struct myfile_data *mfd = malloc(sizeof(struct myfile_data));
	mfd->buf = buf;
	mfd->bufsize = bufsize;

	return mfd;
}

/**
 * Read and return a file from disk or cache
 */
void get_file(int fd, struct cache *cache, char *request_path)
{
	struct cache_entry *ce = cache_get(cache, request_path);
	char *mime_type;
	char path[512] = {0};
	struct myfile_data *file;

	strcat(path, "serverroot/");
	strcat(path, request_path);

	printf("%s\n", path);

	if (ce == NULL) {
		// file = file_load(path);
		struct myfile_data *file = load_file(path);

		if (file == NULL) {
			resp_404(fd);
			return;
		}

		mime_type = mime_type_get(request_path);
		cache_put(cache, request_path, mime_type, file->buf, file->bufsize);
		ce = cache_get(cache, request_path);
		free(file);
	} else {
		time_t now = time(NULL);

		if ((now - ce->created_at) > 60) {
			cache_delete(cache, ce);

			file = load_file(path);

			if (file == NULL) {
				resp_404(fd);
				return;
			}

			mime_type = mime_type_get(request_path);
			cache_put(cache, request_path, mime_type, file->buf, file->bufsize);
			ce = cache_get(cache, request_path);
			free(file);
		}
	}

	cache_print(cache);

	send_response(fd, "HTTP/1.1 200 OK", ce->content_type, ce->content, ce->content_length);
}

/**
 * Search for the end of the HTTP header
 * 
 * "Newlines" in HTTP can be \r\n (carriage return followed by newline) or \n
 * (newline) or \r (carriage return).
 */
char *find_start_of_body(char *header)
{
	int flag = 0;
	
	while (1) {
		if (*header == '\n') {
			if (flag == 1) {
				return header++;
			} else {
				flag = 1;
			}
		} else if (*header == '\r') {
			if (*(header + 1) == '\n') {
				if (flag == 1) {
					return header + 2;
				} else {
					flag = 1;
					header++;
				}
			} else {
				flag = 1;
			}
		} else {
			if (flag == 1)
				flag = 0;
		}

		header++;
	}
}

int get_content_length(char *request)
{
	char line[512];
	memset(line, 0, 512);

	char *tmp = strstr(request, "Content-Length");

	for (int i = 0; i < 512; i++, tmp++) {
		if (*tmp == '\r') {
			break;
		} else {
			line[i] = *tmp;
		}
	}

	int content_length;

	sscanf(line, "Content-Length: %d", &content_length);

	return content_length;
}

void post_save(void *data, int len) {
	FILE *f = fopen("serverroot/hexdump.bin", "w");

	fwrite(data, 1, len, f);

	fclose(f);
}

/**
 * Handle HTTP request and send response
 */
void handle_http_request(int fd, struct cache *cache)
{
	const int request_buffer_size = 65536; // 64K
	char request[request_buffer_size];

	// Read request
	int bytes_recvd = recv(fd, request, request_buffer_size - 1, 0);

	if (bytes_recvd < 0) {
		perror("recv");
		return;
	}

	char line[1024] = {0};
	char path[256] = {0};
	char name[256] = {0};
	for (int i = 0; request[i] != '\n'; i++) {
		line[i] = request[i];
	}

	printf("%s\n", line);

	if (line[0] == 'G') {
		sscanf(line, "GET %s HTTP/1.1", path);
		sscanf(path, "/%s", name);

		if (strcmp(path, "/d20") == 0) {
			get_d20(fd);
		} else if (strcmp(path, "/") == 0) {
			strcat(name, "index.html");
			pthread_mutex_lock(&cachemutex);
			get_file(fd, cache, name);
			pthread_mutex_unlock(&cachemutex);
		} else {
			pthread_mutex_lock(&cachemutex);
			get_file(fd, cache, name);
			pthread_mutex_unlock(&cachemutex);
		}
	} else {
		// Assume it's a POST request for now.

		char *start_of_body = find_start_of_body(request);

		if (*start_of_body == '\0')
			printf("yay\n");
		else {
			int content_length = get_content_length(request);

			post_save(start_of_body, content_length);

			char *response = "{\"status\": \"ok\"}";

			send_response(fd, "HTTP/1.1 200 OK", "text/json", response, strlen(response));
		}
	}
}

void *thread_handle_request(void *data)
{
	struct thread_data *tdata = (struct thread_data *) data;

	handle_http_request(tdata->sockfd, tdata->cache);

	close(tdata->sockfd);
	free(tdata);

	pthread_exit((void *) 0);
}

/**
 * Main
 */
int main(void)
{
	int newfd;
	struct sockaddr_storage their_addr;
	char s[INET6_ADDRSTRLEN];
	pthread_t threadid;

	struct cache *cache = cache_create(50, 0);
	threadid = 0;
	pthread_mutex_init(&cachemutex, NULL);

	// Get a listening socket
	int listenfd = get_listener_socket(PORT);

	if (listenfd < 0) {
		fprintf(stderr, "webserver: fatal error getting listening socket\n");
		exit(1);
	}

	printf("webserver: waiting for connections on port %s...\n", PORT);

	while (1) {
		socklen_t sin_size = sizeof(their_addr);

		newfd = accept(listenfd, (struct sockaddr *) &their_addr, &sin_size);
		if (newfd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
				get_in_addr((struct sockaddr *)&their_addr),
				s, sizeof s);
		printf("[%s:%d]\n", s, ((struct sockaddr_in *) &their_addr)->sin_port);

		struct thread_data *data = malloc(sizeof(struct thread_data));
		data->sockfd = newfd;
		data->cache = cache;
		pthread_create(&threadid, NULL, thread_handle_request, (void *) data);
		threadid++;
	}

	// Unreachable code

	return 0;
}
