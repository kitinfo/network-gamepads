#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "../libs/logger.h"
#include "protocol.h"

#define LISTEN_QUEUE_LENGTH 128


bool send_message(LOGGER log, int sock_fd, void* data, unsigned len) {
	ssize_t bytes = len;
	ssize_t status = 0;

	while (bytes > 0) {
		status = send(sock_fd, data, len, MSG_NOSIGNAL);
		//status = send(sock_fd, data, bytes, 0);

		if (status < 0) {
			logprintf(log, LOG_ERROR, "Failed to send: %s\n", strerror(errno));
			return false;
		}
		logprintf(log, LOG_DEBUG, "%zu of %zu bytes sent (%zu this iteration)\n", status, len, status);

		bytes -= status;
		data += status;
	}


	return true;
}

ssize_t recv_message(LOGGER log, int sock_fd, uint8_t buf[], unsigned len, uint8_t oldbuf[], unsigned oldlen) {
	ssize_t bytes = 0;
	ssize_t status = 0;
	ssize_t length_needed = -1;

	// copy back old buffer
	if (oldbuf && oldlen > 0) {
		int i;
		for (i = 0; i < oldlen; i++) {
			buf[i] = oldbuf[i];
		}
		bytes = oldlen;

		length_needed = get_size_from_command(buf, status);

		if(length_needed < 0) {
			logprintf(log, LOG_ERROR, "Unknown message type %d\n", buf[0]);
			return -1;
		}

		// not enough bytes for getting the proper size.
		if (length_needed == 0) {
			bytes++;
		}
	}

	while (length_needed < 1 || bytes < length_needed) {

		status = recv(sock_fd, buf + bytes, len - bytes, 0);

		if (status < 0) {
			logprintf(log, LOG_ERROR, "recv() failed: %s\n", strerror(errno));
			return -1;
		}

		if (status == 0) {
			logprintf(log, LOG_ERROR, "Connection closed by server\n");
			return -1;
		}

		logprintf(log, LOG_DEBUG, "%d bytes received\n", status);

		if (status > 0 && length_needed < 1) {
			length_needed = get_size_from_command(buf, status);

			if (length_needed < 0) {
				logprintf(log, LOG_ERROR, "Unknown message type 0x%.2x\n", buf[0]);
				return -1;
			}

			// not enough bytes for getting the proper size.
			if (length_needed == 0) {
				bytes++;
			}
		}

		bytes += status;
	}

	return bytes;
}

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
		fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(error));
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

int udp_listener(char* bindhost, char* port){
	int fd = -1, status, yes = 1;
	struct addrinfo hints;
	struct addrinfo* info;
	struct addrinfo* addr_it;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
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

		yes = 1;
		if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(yes)) < 0){
			fprintf(stderr, "Failed to set SO_REUSEADDR on socket\n");
		}

		yes = 1;
		if(setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (void*)&yes, sizeof(yes)) < 0){
			fprintf(stderr, "Failed to set SO_BROADCAST on socket\n");
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
	return fd;
}

