//
//  FZAAppDelegate.m
//  LogViewer
//
//  Created by Graham Lee on 14/06/2012.
//  Copyright (c) 2012 Graham Lee. All rights reserved.
//

#import "FZAAppDelegate.h"
#import <Security/Security.h>
#import <ServiceManagement/ServiceManagement.h>
#import <sys/socket.h>
#import <sys/un.h>

@implementation FZAAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    CFStringRef jobLabel = CFSTR("com.fuzzyaliens.logcat");
    CFDictionaryRef logcatJob = SMJobCopyDictionary(kSMDomainSystemLaunchd, jobLabel);
    if (logcatJob) {
        // the job's already deployed, so there's nothing to do
        CFRelease(logcatJob);
    } else {
        // gain the right to install the helper
        AuthorizationItem authItem = { .name = kSMRightBlessPrivilegedHelper,
            .valueLength = 0,
            .value = NULL,
            .flags = kAuthorizationFlagDefaults };
        AuthorizationRights authRights	= { .count = 1,
            .items = &authItem };
        
        AuthorizationRef authorization = NULL;
        OSStatus authResult = AuthorizationCreate(&authRights,
                                              kAuthorizationEmptyEnvironment,
                                              kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights,
                                              &authorization);
        if (authResult != errAuthorizationSuccess) {
            NSLog(@"couldn't create AuthorizationRef: error %i", authResult);
        } else {
            // got authorization, so deploy the helper
            CFErrorRef error = NULL;
            BOOL blessResult = SMJobBless(kSMDomainSystemLaunchd, jobLabel, authorization, &error);
            AuthorizationFree(authorization, kAuthorizationFlagDefaults);
            if (!blessResult) {
                CFStringRef errorString = CFErrorCopyDescription(error);
                NSLog(@"couldn't install privileged helper: %@", (__bridge id)errorString);
                CFRelease(errorString);
            }
        }
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return 2;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
    switch (row) {
        case 0:
            return @"system.log";
            break;
        case 1:
            return @"install.log";
            break;
        default:
            return nil;
            break;
    }
}

- (IBAction)sourceListItemSelected:(id)sender {
    self.logViewer.string = @"";
    //choose the appropriate command to send
    char command = 0;
    switch ([sender clickedRow]) {
        case 0:
            command = 's';
            break;
        case 1:
            command = 'i';
            break;
        default:
            return;
    }
    //connect to the remote socket
    int socket_descriptor = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (socket_descriptor == -1) {
        NSLog(@"error creating socket: %s", strerror(errno));
        return;
    }
    struct sockaddr_un address = {
        .sun_family = PF_LOCAL,
        .sun_path = "/var/run/logcat.socket",
    };
    if (connect(socket_descriptor, (const struct sockaddr *)&address, sizeof(address)) != 0) {
        NSLog(@"error connecting to socket: %s", strerror(errno));
        goto done;
    }
    //send the command
    size_t bytes_written = send(socket_descriptor, &command, 1, 0);
    if (bytes_written != 1) {
        NSLog(@"couldn't write to socket: %s",strerror(errno));
        goto done;
    }
    size_t bytes_read = 0;
    char *buffer = malloc(4096);
    while((bytes_read = recv(socket_descriptor, buffer, 4096, 0)) > 0) {
        NSString *logContent = [[NSString alloc] initWithBytes: buffer length: bytes_read encoding: NSUTF8StringEncoding];
        self.logViewer.string = [self.logViewer.string stringByAppendingString: logContent];
    }
    free(buffer);
done:
    if (socket_descriptor != -1) {
        close(socket_descriptor);
    }
}

@end
