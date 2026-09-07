// hal_macos.m
// Lightweight native macOS HAL for LVGL – Pure Objective-C / CoreGraphics
// backing. Directly wraps LVGL draw buffer into CGImage and assigns to
// CALayer.contents. Zero GPU command buffers, zero Metal overhead, minimal
// memory footprint.

#include "hal.h"
#include "lvgl/lvgl.h"
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <time.h>

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

static int32_t gDispW = 0;
static int32_t gDispH = 0;
static int32_t gMouseX = 0;
static int32_t gMouseY = 0;
static bool gMouseDown = false;
static uint8_t *gBuf = NULL;
static lv_display_t *gDisp = NULL;
static CALayer *gLayer = nil;

// -------------------------------------------------------------------------
// Tick
// -------------------------------------------------------------------------

static uint32_t macos_tick_cb(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// -------------------------------------------------------------------------
// View & Mouse Tracking
// -------------------------------------------------------------------------

static bool gResizing = false;

static void resize_display(int32_t new_w, int32_t new_h) {
  if (new_w <= 0 || new_h <= 0 || (new_w == gDispW && new_h == gDispH))
    return;

  gDispW = new_w;
  gDispH = new_h;

  size_t buf_size = (size_t)gDispW * gDispH * 4;
  uint8_t *new_buf = (uint8_t *)realloc(gBuf, buf_size);
  if (!new_buf)
    return;
  gBuf = new_buf;

  lv_display_set_resolution(gDisp, gDispW, gDispH);
  lv_display_set_buffers(gDisp, gBuf, NULL, (uint32_t)buf_size,
                         LV_DISPLAY_RENDER_MODE_DIRECT);
}

@interface LVGLView : NSView
@end

@implementation LVGLView

- (BOOL)acceptsFirstResponder {
  return YES;
}
- (BOOL)acceptsFirstMouse:(NSEvent *)event {
  return YES;
}

- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];
  if (gDisp) {
    resize_display((int32_t)newSize.width, (int32_t)newSize.height);
  }
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  for (NSTrackingArea *ta in [self trackingAreas]) {
    [self removeTrackingArea:ta];
  }
  NSTrackingArea *ta = [[NSTrackingArea alloc]
      initWithRect:[self bounds]
           options:(NSTrackingMouseMoved | NSTrackingActiveInKeyWindow |
                    NSTrackingInVisibleRect)
             owner:self
          userInfo:nil];
  [self addTrackingArea:ta];
}

- (void)updateMouse:(NSEvent *)event down:(BOOL)down {
  NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
  NSRect b = [self bounds];
  int32_t x = (int32_t)p.x;
  int32_t y = (int32_t)(b.size.height - p.y);
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= gDispW)
    x = gDispW - 1;
  if (y >= gDispH)
    y = gDispH - 1;
  gMouseX = x;
  gMouseY = y;
  gMouseDown = down;
}

- (void)mouseDown:(NSEvent *)e {
  [self updateMouse:e down:YES];
}
- (void)mouseDragged:(NSEvent *)e {
  [self updateMouse:e down:YES];
}
- (void)mouseUp:(NSEvent *)e {
  [self updateMouse:e down:NO];
}
- (void)mouseMoved:(NSEvent *)e {
  [self updateMouse:e down:NO];
}

@end

// -------------------------------------------------------------------------
// LVGL Flush Callback
// -------------------------------------------------------------------------

static void dummy_release(void *info, const void *data, size_t size) {
  // Buffer is owned by HAL, do nothing
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area,
                     uint8_t *px_map) {
  if (gLayer && gBuf) {
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        NULL, gBuf, gDispW * gDispH * 4, dummy_release);

    // LVGL ARGB8888 in little-endian is BGRA bytes (kCGBitmapByteOrder32Little
    // | kCGImageAlphaPremultipliedFirst)
    CGImageRef image = CGImageCreate(
        gDispW, gDispH, 8, 32, gDispW * 4, colorSpace,
        kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst, provider,
        NULL, false, kCGRenderingIntentDefault);

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    gLayer.contents = (__bridge id)image;
    [CATransaction commit];

    CGImageRelease(image);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpace);
  }
  lv_display_flush_ready(disp);
}

// -------------------------------------------------------------------------
// LVGL Input Read Callback
// -------------------------------------------------------------------------

static void mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  if (!data)
    return;
  data->point.x = gMouseX;
  data->point.y = gMouseY;
  data->state = gMouseDown ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// -------------------------------------------------------------------------
// Public HAL API
// -------------------------------------------------------------------------

lv_display_t *hal_init(int32_t w, int32_t h) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    gDispW = (w > 0) ? w : 800;
    gDispH = (h > 0) ? h : 480;

    lv_tick_set_cb(macos_tick_cb);

    size_t buf_size = (size_t)gDispW * gDispH * 4;
    gBuf = (uint8_t *)malloc(buf_size);
    if (!gBuf)
      return NULL;
    memset(gBuf, 0, buf_size);

    gDisp = lv_display_create(gDispW, gDispH);
    lv_display_set_color_format(gDisp, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(gDisp, gBuf, NULL, (uint32_t)buf_size,
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(gDisp, flush_cb);
    lv_display_set_default(gDisp);

    NSRect frame = NSMakeRect(0, 0, gDispW, gDispH);
    NSWindowStyleMask style =
        NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [win setTitle:@"LVGL Simulator"];
    [win center];

    LVGLView *view = [[LVGLView alloc] initWithFrame:frame];
    [view setWantsLayer:YES];
    gLayer = [CALayer layer];
    gLayer.contentsGravity = kCAGravityResizeAspect;
    gLayer.magnificationFilter = kCAFilterNearest;
    [view setLayer:gLayer];

    [win setContentView:view];
    [win setAcceptsMouseMovedEvents:YES];
    [win makeKeyAndOrderFront:nil];

    lv_group_set_default(lv_group_create());
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, mouse_read_cb);
    lv_indev_set_display(indev, gDisp);
    lv_indev_set_group(indev, lv_group_get_default());

    NSTimer *timer = [NSTimer timerWithTimeInterval:0.005
                                            repeats:YES
                                              block:^(NSTimer *_Nonnull t) {
                                                @autoreleasepool {
                                                  lv_timer_handler();
                                                }
                                              }];
    [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];

    [NSApp activateIgnoringOtherApps:YES];

    NSMenu *mainMenu = [[NSMenu alloc] init];
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:appMenuItem];
    NSMenu *appMenu = [[NSMenu alloc] init];
    NSMenuItem *quitMenuItem =
        [[NSMenuItem alloc] initWithTitle:@"Quit"
                                   action:@selector(terminate:)
                            keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
    [NSApp setMainMenu:mainMenu];

    return gDisp;
  }
}

void hal_run(void) {
  @autoreleasepool {
    [NSApp run];
  }
}
