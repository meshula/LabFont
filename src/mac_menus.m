#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>


#if __has_feature(objc_arc)
#define OBJC_RELEASE(obj) { obj = nil; }
#else
#define OBJC_RELEASE(obj) { [obj release]; obj = nil; }
#endif

void add_quit_menu()
{
    NSMenu* menu_bar = [[NSMenu alloc] init];
    NSMenuItem* app_menu_item = [[NSMenuItem alloc] init];
    [menu_bar addItem:app_menu_item];
    NSApp.mainMenu = menu_bar;
    NSMenu* app_menu = [[NSMenu alloc] init];
    NSMenuItem* quit_menu_item = [[NSMenuItem alloc]
        initWithTitle:@"Quit"
        action:@selector(terminate:)
        keyEquivalent:@"q"];
    [app_menu addItem:quit_menu_item];
    app_menu_item.submenu = app_menu;
    OBJC_RELEASE( app_menu );
    OBJC_RELEASE( app_menu_item );
    OBJC_RELEASE( menu_bar );
}

