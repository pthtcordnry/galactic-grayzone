    #ifndef EDITOR_MODE_H
    #define EDITOR_MODE_H
    
    // State variables for menu status
    extern bool fileMenuOpen;
    extern bool toolsMenuOpen;
    extern bool tilemapSubmenuOpen;
    extern bool entitiesSubmenuOpen;
    
    // Menu item labels
    extern const char *fileItems[3];
    extern const char *tilemapItems[4];
    extern const char *entitiesItems[4];

    void DrawEditor();
    #endif