#pragma once

#import <Cocoa/Cocoa.h>

#include "Dial.h"

/* The window's only view: it draws the dial and the hour's motto, and ticks
   itself once a second. It owns nothing that comes off disk - everything it
   draws is handed to it by -applyTheme:..., which is also what makes
   "Reload Resources" a one-liner. */
@interface DialView : NSView

- (void)applyTheme:(const DialTheme*)Theme
          numerals:(NSArray*)Numerals
            mottos:(NSArray*)Mottos;

/* The title from theme.conf, so the window can name itself. */
- (NSString*)dialTitle;

@end
