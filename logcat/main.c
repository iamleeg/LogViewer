//
//  main.m
//  logcat
//
//  Created by Graham Lee on 14/06/2012.
//  Copyright (c) 2012 Graham Lee. All rights reserved.
//

#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>
#include <fcntl.h>
#include <launch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <Security/Security.h>

void exit_error(const char *message, const int error) __attribute__((noreturn));
int listen_to_launchd_sockets();

int main(int argc, const char * argv[]) {
    int kernel_queue = listen_to_launchd_sockets();
    
    struct kevent listening_event;
    int connected_socket = kevent(kernel_queue, NULL, 0, &listening_event, 1, NULL);
    if (connected_socket == -1) {
        exit_error("couldn't get the connected socket", errno);
    }
    
    //accept the connection
    struct sockaddr accepted_address = { 0 };
    socklen_t address_length = 0;
    
    int accepted_socket = accept((int)listening_event.ident, &accepted_address, &address_length);
    if (accepted_socket == -1) {
        exit_error("couldn't accept the socket connection", errno);
    }
    //read, recreate and test the authorization right
    AuthorizationExternalForm external_form = {0};
    size_t bytes_read = recv(accepted_socket, &external_form, kAuthorizationExternalFormLength, 0);
    if (bytes_read != kAuthorizationExternalFormLength) {
        exit_error("couldn't read the authorization rights from the socket", errno);
    }
    AuthorizationRef authorization = NULL;
    OSStatus auth_result = AuthorizationCreateFromExternalForm(&external_form, &authorization);
    if (auth_result != errAuthorizationSuccess) {
        exit_error("couldn't internalize the authorization reference", 0);
    }
    AuthorizationItem right = { .name = "com.fuzzyaliens.LogViewer.read",
        .valueLength = 0,
        .value = NULL,
        .flags = kAuthorizationFlagDefaults };
    AuthorizationRights right_set = { .count = 1, .items = &right };
    auth_result = AuthorizationCopyRights(authorization,
                                          &right_set,
                                          kAuthorizationEmptyEnvironment,
                                          kAuthorizationFlagDefaults | kAuthorizationFlagExtendRights,
                                          NULL);
    if (auth_result != errAuthorizationSuccess) {
        exit_error("not authorized to read log files", 0);
    }
    AuthorizationFree(authorization, kAuthorizationFlagDefaults);
    //read and check the one-byte command
    char command = 0;
    bytes_read = recv(accepted_socket, &command, 1, 0);
    if (bytes_read != 1) {
        exit_error("couldn't read the command from the socket", errno);
    }
    //write the reply
    int log_file = -1;
    switch (command) {
        case 's': {
            log_file = open("/var/log/system.log", O_RDONLY);
            break;
        }
        case 'i': {
            log_file = open("/var/log/install.log", O_RDONLY);
            break;
        }
        default: {
            exit_error("unknown command", 0);
            break;
        }
    }
    if (log_file == -1) {
        exit_error("couldn't open the log file", errno);
    }
    char *log_content = malloc(4096);
    if (log_content == NULL) {
        exit_error("couldn't allocate memory", errno);
    }
    size_t file_bytes_read = 0;
    while ((file_bytes_read = read(log_file, log_content, 4096)) > 0) {
        ssize_t socket_bytes_written = send(accepted_socket, log_content, file_bytes_read, 0);
        if (socket_bytes_written < file_bytes_read) {
            exit_error("couldn't write to socket", errno);
        }
    }
    close(log_file);
    free(log_content);
    //close the connection
    close(accepted_socket);
    close(kernel_queue);
    return 0;
}

void exit_error(const char *message, const int error) {
    if (error == 0) {
        fprintf(stderr, "%s\n", message);
    } else {
        fprintf(stderr, "%s: %s\n", message, strerror(errno));
    }
    exit(EX_OSERR);
}

int listen_to_launchd_sockets() {
    //check-in with launchd
    launch_data_t checkin_message = launch_data_new_string(LAUNCH_KEY_CHECKIN);
    if (checkin_message == NULL) {
        exit_error("couldn't create launchd checkin message", errno);
    }
    launch_data_t checkin_result = launch_msg(checkin_message);
    if (checkin_result == NULL) {
        exit_error("couldn't check in with launchd", errno);
    }
    if (launch_data_get_type(checkin_result) == LAUNCH_DATA_ERRNO) {
        exit_error("error on launchd checkin",launch_data_get_errno(checkin_result));
        return EX_OSERR;
    }
    launch_data_t socket_info = launch_data_dict_lookup(checkin_result, LAUNCH_JOBKEY_SOCKETS);
    if (socket_info == NULL) {
        exit_error("couldn't find socket information", 0);
    }
    launch_data_t listening_sockets = launch_data_dict_lookup(socket_info, "Listener");
    if (listening_sockets == NULL) {
        exit_error("couldn't find my socket", 0);
    }
    //set up a kevent for our socket
    int kernel_queue = kqueue();
    if (kernel_queue == -1) {
        exit_error("couldn't create kernel queue", errno);
    }
    for (int i = 0; i < launch_data_array_get_count(listening_sockets); i++) {
        launch_data_t this_socket = launch_data_array_get_index(listening_sockets, i);
        struct kevent kev_init;
        EV_SET(&kev_init, launch_data_get_fd(this_socket), EVFILT_READ, EV_ADD, 0, 0, NULL);
        if (kevent(kernel_queue, &kev_init, 1, NULL, 0, NULL) == -1) {
            exit_error("couldn't create kernel event", errno);
        }
    }
    launch_data_free(checkin_result);
    return kernel_queue;
}