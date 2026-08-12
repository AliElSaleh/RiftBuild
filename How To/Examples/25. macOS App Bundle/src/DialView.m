#import "DialView.h"

#include <math.h>

#define TAU 6.28318530717958647692

@implementation DialView
{
    DialTheme Theme;
    NSArray*  Numerals;
    NSArray*  Mottos;
    NSTimer*  Tick;
}

- (instancetype)initWithFrame:(NSRect)Frame
{
    self = [super initWithFrame:Frame];

    if (self != nil)
    {
        Dial_DefaultTheme(&Theme);

        /* The shadow moves with the minute hand, so a second is plenty. */
        Tick = [NSTimer scheduledTimerWithTimeInterval:1.0
                                                target:self
                                              selector:@selector(onTick:)
                                              userInfo:nil
                                               repeats:YES];
    }

    return self;
}

- (void)dealloc
{
    [Tick invalidate];
    [Numerals release];
    [Mottos release];

    [super dealloc];
}

- (void)onTick:(NSTimer*)Timer
{
    (void)Timer;

    [self setNeedsDisplay:YES];
}

- (void)applyTheme:(const DialTheme*)NewTheme
          numerals:(NSArray*)NewNumerals
            mottos:(NSArray*)NewMottos
{
    Theme = *NewTheme;

    [NewNumerals retain];
    [Numerals release];
    Numerals = NewNumerals;

    [NewMottos retain];
    [Mottos release];
    Mottos = NewMottos;

    [self setNeedsDisplay:YES];
}

- (NSString*)dialTitle
{
    return [NSString stringWithUTF8String:Theme.Title];
}

static NSColor* ColorOf(DialColor Color)
{
    return [NSColor colorWithSRGBRed:Color.Red green:Color.Green blue:Color.Blue alpha:1.0];
}

/* Draws Text centred on CentreX, with its top at Top, and returns the height
   it used so the caller can stack the next line under it. */
static CGFloat DrawCentred(NSString* Text, NSFont* Font, NSColor* Color, CGFloat CentreX, CGFloat Top)
{
    NSDictionary* Attributes = @{ NSFontAttributeName            : Font,
                                  NSForegroundColorAttributeName : Color };

    NSSize Size = [Text sizeWithAttributes:Attributes];
    [Text drawAtPoint:NSMakePoint(CentreX - Size.width / 2.0, Top - Size.height) withAttributes:Attributes];

    return Size.height;
}

- (void)drawRect:(NSRect)DirtyRect
{
    (void)DirtyRect;

    NSRect Bounds = [self bounds];

    [ColorOf(Theme.Background) setFill];
    NSRectFill(Bounds);

    /* Text lives in bands at the top and bottom; the dial gets what is left. */
    const CGFloat TopBand    = 64.0;
    const CGFloat BottomBand = 64.0;

    CGFloat BandHeight = MAX(80.0, NSHeight(Bounds) - TopBand - BottomBand);
    CGFloat CentreX = NSMidX(Bounds);
    CGFloat CentreY = BottomBand + BandHeight / 2.0;
    CGFloat Radius  = Theme.Radius * MIN(NSWidth(Bounds), BandHeight);
    Radius = MIN(Radius, MIN(NSWidth(Bounds), BandHeight) / 2.0 - 8.0);

    NSDateComponents* Now = [[NSCalendar currentCalendar] components:NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond
                                                           fromDate:[NSDate date]];
    NSInteger Hour   = [Now hour];
    NSInteger Minute = [Now minute];

    /* The face. */
    NSRect FaceRect = NSMakeRect(CentreX - Radius, CentreY - Radius, 2.0 * Radius, 2.0 * Radius);
    NSBezierPath* Face = [NSBezierPath bezierPathWithOvalInRect:FaceRect];
    [ColorOf(Theme.Face) setFill];
    [Face fill];

    [ColorOf(Theme.Rim) setStroke];
    [Face setLineWidth:MAX(2.0, Radius * 0.05)];
    [Face stroke];

    /* Sixty minute ticks, with every fifth one long enough to read as an hour. */
    [ColorOf(Theme.Mark) setStroke];
    for (int Minute60 = 0; Minute60 < 60; Minute60++)
    {
        BOOL bOnTheHour = (Minute60 % 5) == 0;
        CGFloat Angle = Minute60 * (TAU / 60.0);
        CGFloat Inner = Radius * (bOnTheHour ? 0.86 : 0.90);

        NSBezierPath* TickPath = [NSBezierPath bezierPath];
        [TickPath setLineWidth:bOnTheHour ? MAX(1.5, Radius * 0.02) : 1.0];
        [TickPath moveToPoint:NSMakePoint(CentreX + Inner * sin(Angle), CentreY + Inner * cos(Angle))];
        [TickPath lineToPoint:NSMakePoint(CentreX + Radius * 0.94 * sin(Angle), CentreY + Radius * 0.94 * cos(Angle))];
        [TickPath stroke];
    }

    /* The numerals, straight out of numerals.txt - twelve lines, noon first
       and then clockwise. */
    NSFont* NumeralFont = [NSFont fontWithName:@"Palatino" size:MAX(9.0, Radius * 0.16)];
    if (NumeralFont == nil)
    {
        NumeralFont = [NSFont systemFontOfSize:MAX(9.0, Radius * 0.16)];
    }

    NSDictionary* NumeralAttributes = @{ NSFontAttributeName            : NumeralFont,
                                         NSForegroundColorAttributeName : ColorOf(Theme.Mark) };

    for (NSUInteger i = 0; i < [Numerals count]; i++)
    {
        NSString* Numeral = [Numerals objectAtIndex:i];
        CGFloat Angle = i * (TAU / (CGFloat)[Numerals count]);
        CGFloat Distance = Radius * 0.72;

        NSSize Size = [Numeral sizeWithAttributes:NumeralAttributes];
        NSPoint Point = NSMakePoint(CentreX + Distance * sin(Angle) - Size.width / 2.0,
                                    CentreY + Distance * cos(Angle) - Size.height / 2.0);

        [Numeral drawAtPoint:Point withAttributes:NumeralAttributes];
    }

    /* The shadow: a wedge thrown from the gnomon towards the current time. */
    CGFloat TimeAngle = ((Hour % 12) + Minute / 60.0) * (TAU / 12.0);
    CGFloat Spread = 0.045;
    CGFloat Length = Radius * 0.80;

    NSBezierPath* Shadow = [NSBezierPath bezierPath];
    [Shadow moveToPoint:NSMakePoint(CentreX + Radius * 0.10 * sin(TimeAngle - TAU / 4.0),
                                    CentreY + Radius * 0.10 * cos(TimeAngle - TAU / 4.0))];
    [Shadow lineToPoint:NSMakePoint(CentreX + Length * sin(TimeAngle - Spread),
                                    CentreY + Length * cos(TimeAngle - Spread))];
    [Shadow lineToPoint:NSMakePoint(CentreX + Length * sin(TimeAngle + Spread),
                                    CentreY + Length * cos(TimeAngle + Spread))];
    [Shadow lineToPoint:NSMakePoint(CentreX + Radius * 0.10 * sin(TimeAngle + TAU / 4.0),
                                    CentreY + Radius * 0.10 * cos(TimeAngle + TAU / 4.0))];
    [Shadow closePath];

    [[ColorOf(Theme.Shadow) colorWithAlphaComponent:0.85] setFill];
    [Shadow fill];

    /* The gnomon itself. */
    CGFloat PinRadius = MAX(3.0, Radius * 0.07);
    NSBezierPath* Gnomon = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(CentreX - PinRadius, CentreY - PinRadius,
                                                                            2.0 * PinRadius, 2.0 * PinRadius)];
    [ColorOf(Theme.Gnomon) setFill];
    [Gnomon fill];

    /* Title and version above the dial. Version(define) in the build file put
       this macro here, and the same number is CFBundleShortVersionString. */
    CGFloat Top = NSMaxY(Bounds) - 18.0;
    Top -= DrawCentred([NSString stringWithUTF8String:Theme.Title],
                       [NSFont systemFontOfSize:20.0 weight:NSFontWeightSemibold],
                       ColorOf(Theme.Face), CentreX, Top) + 4.0;

    DrawCentred([NSString stringWithFormat:@"%02ld:%02ld  -  version %s", (long)Hour, (long)Minute, SUNDIAL_VERSION_STRING],
                [NSFont monospacedDigitSystemFontOfSize:12.0 weight:NSFontWeightRegular],
                [ColorOf(Theme.Face) colorWithAlphaComponent:0.55], CentreX, Top);

    /* The hour's motto. */
    if (Hour < (NSInteger)[Mottos count])
    {
        NSFont* MottoFont = [NSFont fontWithName:@"Palatino-Italic" size:15.0];
        if (MottoFont == nil)
        {
            MottoFont = [NSFont systemFontOfSize:15.0];
        }

        DrawCentred([Mottos objectAtIndex:Hour], MottoFont, ColorOf(Theme.Face), CentreX, BottomBand - 20.0);
    }
}

@end
