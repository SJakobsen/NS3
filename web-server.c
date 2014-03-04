#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// PORT that the server will listen to
#define PORT 8080

// Max message size that the server will accept
#define BUFLEN 1500

#define HOST_NAME_MAX 255
#define FILE_NAME_MAX 255

// COMMON HTTP RESPONSES //
// To be expanded upon in later functions
#define RESPONSE_200_HEADER "HTTP/1.1 200 OK\r\n"
// Response when file not found, taken from assignment sheet 
#define RESPONSE_404 "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\r\n3\r\n\"http://www.w3.org/TR/html4/strict.dtd\">\r\n<html>\r\n<head>\r\n<title> 404 Not Found </title>\r\n</head>\r\n<body>\r\n<p> The requested file cannot be found. </p>\r\n</body>\r\n</html>"
// Response when a request cannot be unserstood
#define RESPONSE_400 "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\r\n3\r\n\"http://www.w3.org/TR/html4/strict.dtd\">\r\n<html>\r\n<head>\r\n<title> 400 Bad Request </title>\r\n</head>\r\n<body>\r\n<p> Error 400. Bad Request. </p>\r\n</body>\r\n</html>"
// Response when I mess up
#define RESPONSE_500 "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\r\n3\r\n\"http://www.w3.org/TR/html4/strict.dtd\">\r\n<html>\r\n<head>\r\n<title> 500 Internal Server Errpr </title>\r\n</head>\r\n<body>\r\n<p> Error 500. Internal Server Error, my bad. </p>\r\n</body>\r\n</html>"

// MY HELPER FUNCTIONS //

// msg is message server has received received
// splitmsg points to an array that will contain an array for each line in the newly split message. Lot of stars...
// Lines will be split with the assumption that a line ends end with '\r\n'
// Hence the final header would end with '\r\n\r\n'
int split_HTTP_message(char *msg, char ***splitmsg, int fd);

char *get_filename(char *GETline, int fd);
char *read_file(char *filename, int fd);

int send_200_response(int fd, char *data, char *extension);
void send_400_response(int fd);
void send_404_response(int fd);
void send_500_response(int fd);

int main(void) {
	
	// server
	struct sockaddr_in addr;
	int fd = 0;
	int backlog = 10;
	//client
	struct sockaddr_in cliaddr;
	int connfd = 0;
	
	// CREATE SOCKET
	
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		// an error occurred
		fprintf(stderr, "Error creating socket");
		return -1;
	}
	
	// BIND SOCKET TO PORT

	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);

	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		// an error occurred
		fprintf(stderr, "Error binding socket to port.\n");
		return -1;
	}

	// LISTEN FOR CONNECTIONS

	if (listen(fd, backlog) == -1) {
		// an error occurred
		fprintf(stderr, "Error listening for connections.\n");
		return -1;
	}
	
	printf("Server started.\n");

	// ACCEPTING CONNECTIONS
	
	while (1) {
		socklen_t cliaddr_len = sizeof(cliaddr);

		connfd = accept(fd, (struct sockaddr *) &cliaddr, &cliaddr_len);
		if (connfd == -1) {
			// an error occurred
			fprintf(stderr, "Error accepting connection.\n");
			close(connfd);
			continue;
		}
		else if (connfd == 0) {
			fprintf(stderr, "Connection closed by client.\n");
			close(connfd);
			continue;
		}

		// HANDLE CONNECTION

		ssize_t rcount;
		char buf[BUFLEN];

		rcount = read(connfd, buf, BUFLEN);
		if (rcount == -1) {
			// An error occurred
			fprintf(stderr, "Error printing received data.\n");
		}
		else {
			char **splitmsg;
			// DEBUGGING
			// printf("%s\n", buf);
		
			int linenum = split_HTTP_message(buf, &splitmsg, connfd);
			if (linenum < 0) {
				printf("Failure at split_HTTP_message. HANDLE ME\n");
				close(connfd);
				continue;
			}
		
			char *filename = get_filename(*splitmsg, connfd);
			if (filename == NULL) {
				printf("Failure at get_filename. HANDLE ME\n");
				close(connfd);
				continue;
			}
			// DEBUGGING
			// printf("Parsed file: %s\n", filename);
		
			char *data = read_file(filename, connfd);
			if (data == NULL) {
				printf("Failure at read_file. HANDLE ME\n");
				close(connfd);
				continue;
			}
			// DEBUGGING
			// printf("Read data:\n%s\n", data);
		
			// Read file extension in order to send appropriate response
			char *extension = strrchr(filename, '.');
			if (extension == NULL) {
				fprintf(stderr, "Error reading file extension.");
				free(filename);
				free(data);
				send_400_response(connfd);
				continue;
			}
			else {
				extension++;
			
				// DEBUGGING
				printf("Extension: %s\n", extension);
			
				int success = send_200_response(connfd, data, extension);
				if (!success) {
					fprintf(stderr, "Failed to send 200 response.\n");
					continue;
				}
				else printf("200 response sent.\n");
			
			}
		
		}
		
	}
	
	close(connfd);
	close(fd);
	
	return 0;
	
}

int split_HTTP_message(char *msg, char ***splitmsg, int fd) {
	
	char *current = msg;
	int msglen = strlen(msg);
	int linenum = 0;
	
	// Check for GET request
	/*
	if (strncmp(msg, "GET", 3) == 0) {
		printf("Parsing GET request\n\n");
	}
	*/
	
	// COUNTING NUMBER OF LINES //
	
	int pos = 0;
	int endreached = 0;
	for (; pos<msglen; pos++) {
		if (!strncmp(current, "\r\n", strlen("\r\n"))) {
			linenum++;
			// Consecutive '\r\n' shows end of headers
			if (!strncmp(current + strlen("\r\n"), "\r\n", strlen("\r\n"))) { endreached = 1; break; }
		}
		current++;
	}
	
	// If end of headers does not contain '\r\n\r\n', bad message.
	// If no lines, bad message
	if (!endreached || !linenum) {
		send_400_response(fd);
		fprintf(stderr, "Bad request\n");
		return -1;
	}
	
	// POPULATING SPLITMSG //
	
	*splitmsg = malloc(linenum * sizeof(void));
	
	// Handle malloc error
	if (*splitmsg == NULL) {
		fprintf(stderr, "Error allocating space for splitmsg.\n");
		send_500_response(fd);
		return -1;
	}
	
	// At this point, there should be at least one line to be added to splitmsg
	current = strtok(msg, "\r\n");
	
		// Covering my ass
		if (current == NULL){
			fprintf(stderr, "No delimiter found in HTTP request\n");
			fprintf(stderr,"Request: %s\n", msg);
			free(*splitmsg);
			send_400_response(fd);
			return -1;
		}
		
		**splitmsg = current;
		int i=1;
		for(; i<linenum && current != NULL; i++){
			current = strtok(NULL, "\r\n");
			*(*splitmsg+i) = current;
		}
	
	return linenum;
}

/*
int get_file(char *line) {
	return 0;
}
*/

int check_hostname(char **splitmsg, int lines, int fd) {
	
	// Go through each line looking for "host: "
	char *line;
	int found = 0;
	int i=0;
	for(; i<lines; i++) {
		line = *(splitmsg+i);
		if (!strncmp(line, "host: ", 6)) {
			// Mark as found, line now contains hostname
			found = 1;
			break;
		}
	}
	
	// If host not in headers, don't need to worry about it
	if (!found) return 1;
	
	int check = 0;
	// USING HOST_NAME_MAX
	// SEE THIS ANSWER FROM STACKOVERFLOW
	// http://stackoverflow.com/a/8724971
	char hostname[HOST_NAME_MAX + 1];
	check = gethostname(hostname, HOST_NAME_MAX);
	if (check<0) {
		fprintf(stderr, "Error getting hostname.\n");
		send_500_response(fd);
		return -1;
	}
	
	// If hostnames match return true
	if(!strncmp(line+6, hostname, HOST_NAME_MAX)) return 1;
	
	return 0;
}

char *get_filename(char *GETline, int fd) {
	char *get;
	char *filename;
	char *protocol;
	
	// Allow room for "GET\0"
	get = malloc(sizeof(char)*4);
	if (get == NULL) {
		fprintf(stderr, "Error allocating space for GET request in get_filename.\n");
		free(get);
		send_500_response(fd);
		return NULL;
	}
	
	// Allow extra space for "./" ... "\0"
	// Will be assuming all file paths start in directory of "server"
	filename = malloc(sizeof(char)*(FILE_NAME_MAX+3));
	if (filename == NULL) {
		fprintf(stderr, "Error allocating space for filename in get_filename.\n");
		free(get);
		free(filename);
		send_500_response(fd);
		return NULL;
	}
	
	// Allow room for "HTTP/1.1\0"
	protocol = malloc(sizeof(char)*9);
	if (protocol == NULL) {
		fprintf(stderr, "Error allocating space for protocol in get_filename.\n");
		free(get);
		free(filename);
		free(protocol);
		send_500_response(fd);
		return NULL;
	}
	
	// Split line into the three strings it should be made up of
	// Ref: "GET /index.html HTTP/1.1"
	int check = sscanf(GETline, "%3s %s %s", get, filename, protocol);
	// Skip over "/" in filename.
	filename++;
	// Handle edge case "GET / HTTP/1.1"
	// Alternatively, could set filename to be index.html in this case
	if (strncmp(filename, "", 1) == 0) {
		fprintf(stderr, "Request must include a filename.");
		free(get);
		free(filename);
		free(protocol);
		send_400_response(fd);
		return NULL;
	}
	
	// Expecting 3 arguments, if that's not the case, bad request
	if (check != 3) {
		fprintf(stderr, "Error parsing GET line.  Invalid line, bad arguments.\n");
		free(get);
		free(filename);
		free(protocol);
		send_400_response(fd);
		return NULL;
	}
	
	// Ensure GET request
	if (strncmp(get, "GET", 3) != 0) {
		fprintf(stderr, "Request is not a GET.\n");
		free(get);
		free(filename);
		free(protocol);
		send_400_response(fd);
		return NULL;
	}
	
	free(get);
	free(protocol);
	return filename;
}

char *read_file(char *filename, int fd) {
	FILE *file;
	char *data;
	unsigned long size;		// Size of file being requested
	size_t check;			// Size of read
	
	// This will look for files in same directory as the server, assuming server is compiled into same directory as .c file
	// Reading as a binary file, hence 'rb' (file could be image etc.)
	file = fopen(filename, "rb");
	// Handle fail case (404 error)
	if (file == NULL) {
		perror("");
		fprintf(stderr, "Error opening %s\n", filename);
		send_404_response(fd);
		return NULL;
	}
	
	// obtain file size
	// taken from example at http://www.cplusplus.com/reference/cstdio/fread/
	fseek(file , 0 , SEEK_END);
	size = ftell(file);
	rewind(file);
	
	// Allocate space for file data
	data = malloc(sizeof(char) * (size + 1));
	// Handle fail case
	if (data == NULL) {
		fprintf(stderr, "Error allocating space for data to be written.\n");
		fclose(file);
		free(data);
		send_500_response(fd);
		return NULL;
	}
	
	// Read data from file into data buffer
	check = fread (data, sizeof(char), size, file);
	// Handle misread
	if (check != size) {
		fprintf(stderr, "Error reading from file.\n");
		fclose(file);
		free(data);
		send_500_response(fd);
		return NULL;
	}
	
	fclose(file);
	return data;
}

// HTTP RESPONSES //

int send_200_response(int fd, char *data, char *extension) {
	int size;
	int check;
	char *contentlength;
	char *contenttype;
	
	contentlength = malloc((18 + 10) * sizeof(char));
	if (contentlength == NULL) {
		fprintf(stderr, "Error allocating space for content length header.");
		free(contentlength);
		send_500_response(fd);
		return 0;
	}
	
	size = strlen(data);
	check = sprintf(contentlength, "Content-Length: %i\r\n", size);
	
	if (!strncmp(extension, "html", 4)) contenttype = "Content-Type: text/html\r\n\r\n";
	else if (!strncmp(extension, "htm", 3)) contenttype = "Content-Type: text/html\r\n\r\n";
	else if (!strncmp(extension, "txt", 3)) contenttype = "Content-Type: text/plain\r\n\r\n";
	else if (!strncmp(extension, "jpeg", 4)) contenttype = "Content-Type: image/jpeg\r\n\r\n";
	else if (!strncmp(extension, "jpg", 3)) contenttype = "Content-Type: image/jpeg\r\n\r\n";
	else if (!strncmp(extension, "gif", 3)) contenttype = "Content-Type: image/gif\r\n\r\n";
	else contenttype = "Content-Type: application/octet-stream\r\n\r\n";

	printf("Content length header: %s", contentlength);
	printf("Content type header: %s", contenttype);
	
	char *response = malloc(sizeof(RESPONSE_200_HEADER) + sizeof(contentlength) + sizeof(contenttype) + size);
	if (response == NULL) {
		fprintf(stderr, "Error allocating space for HTTP 200 Response.");
		free(contentlength);
		free(response);
		send_500_response(fd);
		return 0;
	}
	
	strcpy(response, RESPONSE_200_HEADER);
	strcat(response, contentlength);
	strcat(response, contenttype);
	
	int current = strlen(response);
	memcpy(response+current, data, size+1);
	
	check = write(fd, response, current+size);
	if (check < 0) {
		fprintf(stderr, "Error sending 200 response.");
		free(contentlength);
		free(response);
		send_500_response(fd);
		return 0;
	}
	
	// DEBUGGING
	printf("\n////// 200 response: //////\n");
	printf("%s\n", response);
	
	free(contentlength);
	free(response);
	return 1;
}

void send_400_response(int fd) {
	printf("////// Sending 400 response. //////\n");
	int check;
	check = write(fd, RESPONSE_400, strlen(RESPONSE_400));
	if(check<0){
		fprintf(stderr, "Error sending 400 response.\n");
	}
	close(fd);
	return;
}

void send_404_response(int fd) {
	printf("////// Sending 404 response. //////\n");
	int check;
	check = write(fd, RESPONSE_404, strlen(RESPONSE_404));
	if(check<0){
		fprintf(stderr, "Error sending 404 response.\n");
	}
	close(fd);
	return;
}

void send_500_response(int fd) {
	printf("////// Sending 500 response. //////\n");
	int check;
	check = write(fd, RESPONSE_500, strlen(RESPONSE_500));
	if(check<0){
		fprintf(stderr, "Error sending 500 response.\n");
	}
	close(fd);
	return;
}