#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

// PORT that the server will listen to
#define PORT 8080

// Max message size that the server will accept
#define BUFLEN 1500

#define HOST_NAME_MAX 255

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

void send_200_response(int fd);
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
		printf("Error creating socket");
	}
	
	// BIND SOCKET TO PORT

	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);

	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		// an error occurred
		printf("Error binding socket to port.\n");
	}

	// LISTEN FOR CONNECTIONS

	if (listen(fd, backlog) == -1) {
		// an error occurred
		printf("Error listening for connections.\n");
	}

	// ACCEPTING CONNECTIONS
	
	socklen_t cliaddr_len = sizeof(cliaddr);

	connfd = accept(fd, (struct sockaddr *) &cliaddr, &cliaddr_len);
	if (connfd == -1) {
		// an error occurred
		printf("Error accepting connection.\n");
	}

	// HANDLE CONNECTION

	ssize_t rcount;
	char buf[BUFLEN];

	rcount = read(connfd, buf, BUFLEN);
	if (rcount == -1) {
		// An error occurred
		printf("Error printing received data.\n");
	}
	else {
		char **splitmsg;
		// Print message to console
		printf("%s\n", buf);
		
		int linenum = split_HTTP_message(buf, &splitmsg, connfd);
		
		/*
		int i = 0;
		for(; i<linenum; i++){
			printf("%s\n", *(splitmsg+i));
		}
		*/
		
		free(splitmsg);
	}
	
	close(connfd);
	close(fd);
	
	return 0;
	
}

// message is message server has received received
// splitmsg points to an array that will contain an array for each line in the newly split message.  Lot of stars...
// Lines will be split with the assumption that a line ends end with '\r\n'
// Hence the final header would end with '\r\n\r\n'
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
		printf("Bad request\n");
		return -1;
	}
	
	// POPULATING SPLITMSG //
	
	*splitmsg = malloc(linenum * sizeof(void));
	
	// Handle malloc error
	if (*splitmsg == NULL) {
		printf("Error allocating space for splitmsg");
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
		fprintf(stderr, "Error getting hostname.");
		send_500_response(fd);
		return -1;
	}
	
	// If hostnames match return true
	if(!strncmp(line+6, hostname, HOST_NAME_MAX)) return 1;
	
	return 0;
}

void handle_GET(char **splitmsg, int lines, int fd) {
	// This line should contain a GET request
	char *line = *splitmsg;

	if(!strncmp(line, "GET ", 4)) {
		// TODO
	}
	// Treating a failure here as a bad request - from the specifications of the task
	// Though in reality, a POST would have been a valid request that would be rejected
	// The alternative would be sending an HTTP 500 response, because that's kinda my fault
	else {
		fprintf(stderr, "Error handling GET request.");
		send_400_response(fd);
		return;
	}
}

void send_200_response(int fd) {
	
	close(fd);
	
}

void send_400_response(int fd) {
	int check;
	check = write(fd, RESPONSE_400, strlen(RESPONSE_400));
	if(check<0){
		fprintf(stderr, "Error sending response: 400\n");
	}
	close(fd);
	
}

void send_404_response(int fd) {
	int check;
	check = write(fd, RESPONSE_404, strlen(RESPONSE_404));
	if(check<0){
		fprintf(stderr, "Error sending response: 404\n");
	}
	close(fd);
	
}

void send_500_response(int fd) {
	int check;
	check = write(fd, RESPONSE_500, strlen(RESPONSE_500));
	if(check<0){
		fprintf(stderr, "Error sending response: 500\n");
	}
	close(fd);
}