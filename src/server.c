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
#include "net.h"
#include "file.h"
#include "mime.h"
#include "cache.h"

#define PORT "3490"  // the port users will be connecting to

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

	strcat(response, header);
	strcat(response, "\n");

	strcat(response, "Connection: Close\n");

	sprintf(buf, "Content-Type: %s\n", content_type);
	strcat(response, buf);

	sprintf(buf, "Content-Length: %d\n", content_length);
	strcat(response, buf);

	sprintf(buf, "Cache-Control: no-cache\n");
	strcat(response, buf);

	strcat(response, "\n");

	printf("%s", response);

	int response_length = strlen(response);

	char *arr = (char *) body;
	for (int i = strlen(response), j = 0; j < content_length; j++, i++) {
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
	snprintf(filepath, sizeof filepath, "%s/404.html", SERVER_FILES);
	filedata = file_load(filepath);

	if (filedata == NULL) {
		// TODO: make this non-fatal
		fprintf(stderr, "cannot find system 404 file\n");
		exit(3);
	}

	mime_type = mime_type_get(filepath);

	send_response(fd, "HTTP/1.1 404 NOT FOUND", mime_type, filedata->data, filedata->size);

	file_free(filedata);
}

/**
 * Read and return a file from disk or cache
 */
void get_file(int fd, struct cache *cache, char *request_path)
{
	struct cache_entry *ce = cache_get(cache, request_path);

	if (ce == NULL) {
		long int bufsize;
		unsigned char *buf;
		char *mime_type;
		char path[512] = {0};

		strcat(path, "serverroot/");
		strcat(path, request_path);

		printf("%s\n", path);

		FILE *f = fopen(path, "r");

		if (f == NULL) {
			resp_404(fd);
			return;
		}

		fseek(f, 0L, SEEK_END);
		bufsize = ftell(f);
		fseek(f, 0L, SEEK_SET);

		buf = malloc(bufsize * sizeof(char));
		fread(buf, 1, bufsize, f);

		fclose(f);

		mime_type = mime_type_get(request_path);
		cache_put(cache, request_path, mime_type, buf, bufsize);
		ce = cache_get(cache, request_path);
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
	///////////////////
	// IMPLEMENT ME! // (Stretch)
	///////////////////
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
	int i;
	for (i = 0; request[i] != '\n'; i++) {
		line[i] = request[i];
	}

	printf("-----\n");
	printf("%s\n", line);
	printf("-----\n");

	if (line[0] == 'G') {
		int rv = sscanf(line, "GET /%s HTTP/1.1", path);

		if (strcmp(path, "d20") == 0) {
			get_d20(fd);
		} else {
			get_file(fd, cache, path);
		}
	}

	// If GET, handle the get endpoints

	//    Check if it's /d20 and handle that special case
	//    Otherwise serve the requested file by calling get_file()


	// (Stretch) If POST, handle the post request
}

/**
 * Main
 */
int main(void)
{
	int newfd;  // listen on sock_fd, new connection on newfd
	struct sockaddr_storage their_addr; // connector's address information
	char s[INET6_ADDRSTRLEN];

	struct cache *cache = cache_create(3, 0);

	// Get a listening socket
	int listenfd = get_listener_socket(PORT);

	if (listenfd < 0) {
		fprintf(stderr, "webserver: fatal error getting listening socket\n");
		exit(1);
	}

	printf("webserver: waiting for connections on port %s...\n", PORT);

	// This is the main loop that accepts incoming connections and
	// responds to the request. The main parent process
	// then goes back to waiting for new connections.

	while(1) {
		socklen_t sin_size = sizeof their_addr;

		// Parent process will block on the accept() call until someone
		// makes a new connection:
		newfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);
		if (newfd == -1) {
			perror("accept");
			continue;
		}

		// Print out a message that we got the connection
		inet_ntop(their_addr.ss_family,
				get_in_addr((struct sockaddr *)&their_addr),
				s, sizeof s);
		printf("server: got connection from %s\n", s);

		// newfd is a new socket descriptor for the new connection.
		// listenfd is still listening for new connections.

		handle_http_request(newfd, cache);

		close(newfd);
		printf("server: closed connection from %s\n", s);
	}

	// Unreachable code

	return 0;
}

