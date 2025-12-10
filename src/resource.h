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
#define ID_EDIT_TIMEDATE                1107

// View menu
#define ID_VIEW_WORDWRAP                1110

// Tools menu (Phase 2)
#define ID_TOOLS_EXECUTEFILTER          1201
#define ID_TOOLS_MANAGEFILTERS          1202
#define ID_TOOLS_FILTER_BASE            1300  // Base for dynamic filter menu items (1300-1399)

// Help menu
#define ID_HELP_ABOUT                   1901

// Dialog IDs
#define IDD_ABOUT                       300
#define IDC_STATIC                      -1

// String resource IDs (2000-2099)
#define IDS_ERROR                       2000
#define IDS_RICHEDIT_LOAD_FAILED        2001
#define IDS_WINDOW_REG_FAILED           2002
#define IDS_WINDOW_CREATE_FAILED        2003
#define IDS_RICHEDIT_CREATE_FAILED      2004
#define IDS_SAVE_CHANGES_PROMPT         2005
#define IDS_NO_TEXT_TO_PROCESS          2006
#define IDS_FILTER_EXECUTION            2007
#define IDS_PIPE_CREATE_FAILED          2008
#define IDS_FILTER_EXEC_ERROR           2009
#define IDS_FILTER_ERROR                2010
#define IDS_FILTER_RESULT               2011
#define IDS_FILTER_HELP_TITLE           2012
#define IDS_FILTER_HELP_TEXT            2013

#endif // RESOURCE_H
