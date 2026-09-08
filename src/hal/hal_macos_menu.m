// hal_macos_menu.m
#import "hal_macos_menu.h"

void macos_setup_menu(void) {
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
}
