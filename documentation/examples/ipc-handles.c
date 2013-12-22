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
 * @brief		IPC handle passing example code.
 *
 * This example demonstrates the ability to send and receive handles to kernel
 * objects over an IPC connection. This example is based on how user
 * authentication will work - client passes authentication information to the
 * security server, which returns a token object containing a security context
 * for the new user.
 */

#include <kernel/ipc.h>
#include <kernel/process.h>
#include <kernel/security.h>
#include <kernel/status.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Security server protocol message IDs. */
#define SECURITY_MESSAGE_AUTH		1
#define SECURITY_MESSAGE_AUTH_REPLY	2

/** Authentication request structure. */
typedef struct auth_request {
	char user[64];			/**< User name. */
	char password[64];		/**< Password. */
} auth_request_t;

/** See ipc-svcmgr.c. */
extern status_t connect_to_service(const char *name, handle_t *connp);

/**
 * Client.
 */

int main(int argc, char **argv) {
	handle_t conn, token;
	ipc_message_t msg;
	auth_request_t auth;
	status_t ret;

	ret = connect_to_service("org.kiwi.SecurityServer", &conn);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to connect to service: %d\n", ret);
		return EXIT_FAILURE;
	}

	snprintf(auth.user, sizeof(auth.user), "rainbowdash");
	snprintf(auth.password, sizeof(auth.password), "isbestpony");

	memset(&msg, 0, sizeof(msg));
	msg.id = SECURITY_MESSAGE_AUTH;
	msg.size = sizeof(auth);

	ret = kern_connection_send(conn, &msg, &auth, INVALID_HANDLE);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to send message: %d\n", ret);
		return EXIT_FAILURE;
	}

	ret = kern_connection_receive(conn, &msg, -1);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to receive message: %d\n", ret);
		return EXIT_FAILURE;
	}

	if(msg.id != SECURITY_MESSAGE_AUTH_REPLY) {
		fprintf(stderr, "Received invalid message\n");
		return EXIT_FAILURE;
	}

	if(msg.args[0] != STATUS_SUCCESS) {
		fprintf(stderr, "Authentication failed: %ld\n", msg.args[0]);
		return EXIT_FAILURE;
	}

	/* The received message has the IPC_MESSAGE_HANDLE flag set to indicate
	 * that a handle was attached by the sender. */
	if(!(msg.flags & IPC_MESSAGE_HANDLE)) {
		fprintf(stderr, "Reply did not contain a handle\n");
		return EXIT_FAILURE;
	}

	/* This function gets the handle attached to the last message received
	 * on a connection. Pending data/handles are dropped when any operation
	 * takes place on this end of the connection other than receive_data()
	 * or receive_handle() (i.e. another receive(), or a send()). */
	ret = kern_connection_receive_handle(conn, &token);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to receive handle: %d\n", ret);
		return EXIT_FAILURE;
	}

	kern_handle_close(conn);

	if(kern_object_type(token) != OBJECT_TYPE_TOKEN) {
		fprintf(stderr, "Received object was not a token\n");
		return EXIT_FAILURE;
	}

	ret = kern_process_set_token(token);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to set process token: %d\n", ret);
		return EXIT_FAILURE;
	}

	/* This process' identity is now the user we authenticated as. */

	return EXIT_SUCCESS;
}

/**
 * Server.
 */

extern status_t auth_user(auth_request_t *auth, security_context_t *ctx);

/**
 * Handle an authentication message.
 *
 * This function is called from the main event loop when an authentication
 * message is received on a connection.
 *
 * @param conn		Handle to the connection.
 * @param msg		Message that was received.
 */
void handle_auth_message(handle_t conn, ipc_message_t *msg) {
	auth_request_t auth;
	security_context_t ctx;
	handle_t token;
	status_t ret;

	ret = kern_connection_receive_data(conn, &auth);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to receive data: %d\n", ret);
		return;
	}

	auth.user[sizeof(auth.user) - 1] = 0;
	auth.password[sizeof(auth.password) - 1] = 0;

	ret = auth_user(&auth, &ctx);
	if(ret == STATUS_SUCCESS) {
		ret = kern_token_create(&ctx, &token);
		if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to create token: %d\n", ret);
			return;
		}
	}

	msg->id = SECURITY_MESSAGE_AUTH_REPLY;
	msg->args[0] = ret;
	msg->flags = (ret == STATUS_SUCCESS) ? IPC_MESSAGE_HANDLE : 0;

	ret = kern_connection_send(conn, msg, NULL, token);
	if(ret != STATUS_SUCCESS)
		fprintf(stderr, "Failed to send message: %d\n", ret);
}
