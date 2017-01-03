#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define LISTEN_QUEUE_LENGTH 128

int tcp_connect(char* host, char* port){
	int sockfd = -1, error;
	struct addrinfo hints;
	struct addrinfo* head;
	struct addrinfo* iter;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	error = getaddrinfo(host, port, &hints, &head);
	if(error){
		fprintf(stderr, "getaddrinfo: %s\r\n", gai_strerror(error));
		return -1;
	}

	for(iter = head; iter; iter = iter->ai_next){
		sockfd = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
		if(sockfd < 0){
			continue;
		}

		error = connect(sockfd, iter->ai_addr, iter->ai_addrlen);
		if(error != 0){
			close(sockfd);
			continue;
		}

		break;
	}

	freeaddrinfo(head);
	iter = NULL;

	if(sockfd < 0){
		perror("socket");
		return -1;
	}

	if(error != 0){
		perror("connect");
		return -1;
	}

	return sockfd;
}

int tcp_listener(char* bindhost, char* port){
	int fd = -1, status, yes = 1;
	struct addrinfo hints;
	struct addrinfo* info;
	struct addrinfo* addr_it;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	status = getaddrinfo(bindhost, port, &hints, &info);
	if(status){
		fprintf(stderr, "Failed to get socket info for %s port %s: %s\n", bindhost, port, gai_strerror(status));
		return -1;
	}

	for(addr_it = info; addr_it != NULL; addr_it = addr_it->ai_next){
		fd = socket(addr_it->ai_family, addr_it->ai_socktype, addr_it->ai_protocol);
		if(fd < 0){
			continue;
		}

		if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&yes, sizeof(yes)) < 0){
			fprintf(stderr, "Failed to set IPV6_V6ONLY on socket for %s port %s: %s\n", bindhost, port, strerror(errno));
		}

		yes = 1;
		if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(yes)) < 0){
			fprintf(stderr, "Failed to set SO_REUSEADDR on socket\n");
		}

		status = bind(fd, addr_it->ai_addr, addr_it->ai_addrlen);
		if(status < 0){
			close(fd);
			continue;
		}

		break;
	}

	freeaddrinfo(info);

	if(!addr_it){
		fprintf(stderr, "Failed to create listening socket for %s port %s\n", bindhost, port);
		return -1;
	}

	status = listen(fd, LISTEN_QUEUE_LENGTH);
	if(status < 0){
		perror("listen");
		close(fd);
		return -1;
	}

	return fd;
}
