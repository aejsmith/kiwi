/*
 * Copyright (C) 2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		IPC basic messaging example code.
 */

#include <kernel/ipc.h>
#include <kernel/status.h>
#include <kernel/thread.h>

#include <stdio.h>
#include <stdlib.h>

/** Test protocol message IDs. */
#define TEST_MESSAGE_PING	1
#define TEST_MESSAGE_PONG	2

/** Test message structure. */
typedef struct test_message {
	char str[128];
} test_message_t;

/** See ipc-svcmgr.c. */
extern status_t connect_to_service(const char *name, handle_t *connp);

/**
 * Client.
 */

int main(int argc, char **argv) {
	handle_t conn;
	ipc_message_t msg;
	test_message_t data;
	unsigned long count = 0;
	status_t ret;

	ret = connect_to_service("org.kiwi.TestService", &conn);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to connect to service: %d\n", ret);
		return EXIT_FAILURE;
	}

	while(true) {
		snprintf(data.str, sizeof(data.str), "PING %lu\n", count);
		msg.id = TEST_MESSAGE_PING;
		msg.size = sizeof(data);
		msg.args[0] = count;

		ret = kern_connection_send(conn, &msg, &data, INVALID_HANDLE);
		if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to send message: %d\n", ret);
			return EXIT_FAILURE;
		}

		ret = kern_connection_receive(conn, &msg, -1);
		if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to receive message: %d\n", ret);
			return EXIT_FAILURE;
		}

		if(msg.id != TEST_MESSAGE_PONG || msg.size != sizeof(data)) {
			fprintf(stderr, "Received invalid message\n");
			return EXIT_FAILURE;
		} else if(msg.args[0] != count) {
			fprintf(stderr, "Received message with incorrect count\n");
			return EXIT_FAILURE;
		}

		/* This function gets the data attached to the last message
		 * received on a connection. The size of the data is given by
		 * msg.size in the received message. Pending data/handles are
		 * dropped when any operation takes place on this end of the
		 * connection other than receive_data() or receive_handle()
		 * (i.e. another receive(), or a send()). */
		ret = kern_connection_receive_data(conn, &data);
		if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to receive data: %d\n", ret);
			return EXIT_FAILURE;
		}

		data.str[sizeof(data.str) - 1] = 0;
		printf("%s\n", data.str);

		count++;
		kern_thread_sleep(secs_to_nsecs(1), NULL);
	}
}

/**
 * Server.
 */

extern status_t register_service(const char *name, handle_t port);

int main(int argc, char **argv) {
	handle_t port, conn;
	ipc_client_t client;
	ipc_message_t msg;
	test_message_t data;
	status_t ret;

	/* Create a port. A port provides a point of connection to a service.
	 * It can only be listened on by the process which creates it. Any
	 * process with a handle to it is able to connect to it. */
	ret = kern_port_create(&port);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to create port: %d\n", ret);
		return EXIT_FAILURE;
	}

	/* Register the port with the service manager. This would transfer a
	 * handle to the port in a message to the service manager. */
	ret = register_service("org.kiwi.TestService", port);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to register service: %d\n", ret);
		return EXIT_FAILURE;
	}

	/* Wait for a connection. */
	ret = kern_port_listen(port, NULL, &client, -1, &conn);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to listen for connection: %d\n", ret);
		return EXIT_FAILURE;
	}

	/* Upon receiving the connection we can get an ipc_client_t structure
	 * filled in which contains the client's PID and a copy of the security
	 * context of the thread that made the connection. This can be used to
	 * do security checks. Connection handles are non-transferrable, meaning
	 * that once fully set up, the processes on each end of a connection
	 * cannot ever change for the lifetime of the connection. In this
	 * example we just accept the connection - see ipc-svcmgr.c for an
	 * example of rejecting and forwarding a connection. */
	kern_connection_accept(conn);

	while(true) {
		ret = kern_connection_receive(conn, &msg, -1);
		if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to receive message: %d\n", ret);
			return EXIT_FAILURE;
		}

		if(msg.id != TEST_MESSAGE_PONG || msg.size != sizeof(data)) {
			fprintf(stderr, "Received invalid message\n");
			return EXIT_FAILURE;
		}

		ret = kern_connection_receive_data(conn, &data);
		if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to receive data: %d\n", ret);
			return EXIT_FAILURE;
		}

		data.str[sizeof(data.str) - 1] = 0;
		printf("%s\n", data.str);

		snprintf(data.str, sizeof(data.str), "PONG %lu\n", msg.args[0]);
		msg.id = TEST_MESSAGE_PONG;
		msg.size = sizeof(data);

		ret = kern_connection_send(conn, &msg, &data, INVALID_HANDLE);
		if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to send message: %d\n", ret);
			return EXIT_FAILURE;
		}
	}
}
