#ifndef RESOURCE_H
#define RESOURCE_H

// Control IDs
#define IDC_RICHEDIT                    100
#define IDC_STATUSBAR                   101

// Menu resource
#define IDR_MENU_MAIN                   200
#define IDR_ACCELERATOR                 201

// File menu
#define ID_FILE_NEW                     1001
#define ID_FILE_OPEN                    1002
#define ID_FILE_SAVE                    1003
#define ID_FILE_SAVEAS                  1004
#define ID_FILE_EXIT                    1005

// Edit menu
#define ID_EDIT_UNDO                    1101
#define ID_EDIT_REDO                    1102
#define ID_EDIT_CUT                     1103
#define ID_EDIT_COPY                    1104
#define ID_EDIT_PASTE                   1105
#define ID_EDIT_SELECTALL               1106

// Tools menu (Phase 2)
#define ID_TOOLS_EXECUTEFILTER          1201
#define ID_TOOLS_MANAGEFILTERS          1202
#define ID_TOOLS_FILTER_BASE            1300  // Base for dynamic filter menu items (1300-1399)

// Help menu
#define ID_HELP_ABOUT                   1901

// Dialog IDs
#define IDD_ABOUT                       300
#define IDC_STATIC                      -1

#endif // RESOURCE_H
