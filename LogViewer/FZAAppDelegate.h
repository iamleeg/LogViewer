//
//  FZAAppDelegate.h
//  LogViewer
//
//  Created by Graham Lee on 14/06/2012.
//  Copyright (c) 2012 Graham Lee. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface FZAAppDelegate : NSObject <NSApplicationDelegate, NSTableViewDataSource, NSTableViewDelegate>

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet NSTableView *sourceList;
@property (assign) IBOutlet NSTextView *logViewer;

- (IBAction)sourceListItemSelected: (id)sender;

@end
