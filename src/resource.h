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
#define IDS_UNTITLED                    2014
#define IDS_NO_FILTERS_CONFIGURED       2015

// File error strings (2016-2024)
#define IDS_ERROR_OPEN_FILE             2016
#define IDS_ERROR_GET_FILE_SIZE         2017
#define IDS_ERROR_OUT_OF_MEMORY         2018
#define IDS_ERROR_READ_FILE             2019
#define IDS_ERROR_CONVERT_ENCODING      2020
#define IDS_ERROR_CONVERT_TEXT_ENCODING 2021
#define IDS_ERROR_CREATE_FILE           2022
#define IDS_ERROR_WRITE_FILE            2023
#define IDS_ERROR_PREFIX                2024

// Filter execution strings (2025-2034)
#define IDS_NO_FILTER_SELECTED          2025
#define IDS_NO_FILTER_SELECTED_MSG      2026
#define IDS_FILTER_EXEC_FAILED          2027
#define IDS_FILTER_STDERR_OUTPUT        2028
#define IDS_FILTER_EXIT_CODE            2029
#define IDS_CONTEXT_UNDO                2030
#define IDS_CONTEXT_CUT                 2031
#define IDS_CONTEXT_COPY                2032
#define IDS_CONTEXT_PASTE               2033
#define IDS_CONTEXT_SELECT_ALL          2034

// File dialog strings (2035-2036)
#define IDS_FILE_FILTER_TEXT            2035
#define IDS_FILE_FILTER_ALL             2036

// Status bar strings (2037-2042)
#define IDS_STATUS_LINE                 2037
#define IDS_STATUS_COLUMN               2038
#define IDS_STATUS_CHAR                 2039
#define IDS_STATUS_DEC                  2040
#define IDS_STATUS_EOF                  2041
#define IDS_STATUS_INVALID_SURROGATE    2042

// Undo/Redo operation type strings (2043-2053)
#define IDS_UNDO                        2043
#define IDS_REDO                        2044
#define IDS_UNDO_TYPING                 2045
#define IDS_UNDO_CUT                    2046
#define IDS_UNDO_PASTE                  2047
#define IDS_UNDO_DELETE                 2048
#define IDS_UNDO_FILTER                 2049
#define IDS_REDO_TYPING                 2050
#define IDS_REDO_CUT                    2051
#define IDS_REDO_PASTE                  2052
#define IDS_REDO_DELETE                 2053
#define IDS_REDO_FILTER                 2054

#endif // RESOURCE_H
