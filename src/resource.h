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
#define ID_FILE_NEW_BLANK               1006  // New blank document (default)
#define ID_FILE_NEW_TEMPLATE_BASE       8000  // Base for File→New template items (8000-8031)
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
#define ID_URL_OPEN                     1108
#define ID_URL_COPY                     1109

// View menu
#define ID_VIEW_WORDWRAP                1110

// Tools menu (Phase 2)
#define ID_TOOLS_EXECUTEFILTER          1201
#define ID_TOOLS_FILTER_HELP          1202
#define ID_TOOLS_START_INTERACTIVE      1090  // Start Interactive Mode (Phase 2.5b)
#define ID_TOOLS_EXIT_INTERACTIVE       1091  // Exit Interactive Mode (Phase 2.5b)
#define ID_TOOLS_FILTER_BASE            1300  // Base for dynamic filter menu items (1300-1399)
#define ID_TOOLS_INSERT_TEMPLATE        1203  // Insert Template submenu
#define ID_TOOLS_SHOW_TEMPLATE_DESCRIPTIONS 1204  // Toggle template descriptions in menu
#define ID_TOOLS_TEMPLATE_BASE          7000  // Base for template menu items (7000-7099)

// File menu - MRU list
#define ID_FILE_MRU_BASE                6000  // Base for MRU menu items (6000-6009)

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

// Undo/Redo operation type strings (2043-2056)
#define IDS_UNDO                        2043
#define IDS_REDO                        2044
#define IDS_UNDO_TYPING                 2045
#define IDS_UNDO_CUT                    2046
#define IDS_UNDO_PASTE                  2047
#define IDS_UNDO_DELETE                 2048
#define IDS_UNDO_FILTER                 2049
#define IDS_UNDO_DRAGDROP               2050
#define IDS_REDO_TYPING                 2051
#define IDS_REDO_CUT                    2052
#define IDS_REDO_PASTE                  2053
#define IDS_REDO_DELETE                 2054
#define IDS_REDO_FILTER                 2055
#define IDS_REDO_DRAGDROP               2056

// URL context menu and error strings (2057-2059)
#define IDS_CONTEXT_OPEN_URL            2057
#define IDS_CONTEXT_COPY_URL            2058
#define IDS_ERROR_OPEN_URL              2059

// EN_STOPNOUNDO notification strings
#define IDS_UNDO_BUFFER_FULL_TITLE      2060
#define IDS_UNDO_BUFFER_FULL_MESSAGE    2061

// REPL mode strings (Phase 2.5b) (2070-2087)
#define IDS_REPL_EXITED                 2070
#define IDS_REPL_ALREADY_ACTIVE         2072
#define IDS_REPL_NOT_ONESHOT            2074
#define IDS_REPL_SWITCH_PROMPT          2076
#define IDS_REPL_CLOSE_PROMPT           2078
#define IDS_CONFIRM                     2080
#define IDS_STATUS_INTERACTIVE          2082
#define IDS_STATUS_FILTER               2084
#define IDS_STATUS_FILTER_NONE          2086
#define IDS_INFORMATION                 2088

// Resume file strings (Phase 2.6)
#define IDS_RESUMED                     2089

// REPL error and localization strings (Phase 2.7)
#define IDS_REPL_FAILED_START           2090
#define IDS_REPL_FAILED_PIPE_STDOUT     2092
#define IDS_REPL_FAILED_PIPE_STDIN      2094
#define IDS_REPL_FAILED_PIPE_STDERR     2096
#define IDS_REPL_FAILED_THREAD_STDOUT   2098
#define IDS_REPL_FAILED_THREAD_STDERR   2100
#define IDS_INTERACTIVE_MODE_INDICATOR  2102
#define IDS_FILTER_RESULT_TITLE         2104

// Template system strings (2106-2124)
#define IDS_BLANK_DOCUMENT              2106
#define IDS_MARKDOWN_DOCUMENT           2108
#define IDS_TEXT_DOCUMENT               2110
#define IDS_HTML_DOCUMENT               2112
#define IDS_NO_TEMPLATES                2114
#define IDS_NO_TEMPLATES_FOR_FILETYPE   2116
#define IDS_MENU_INSERT_TEMPLATE        2118
#define IDS_MENU_NEW                    2120
#define IDS_FILES                       2122
#define IDS_TEXT_FILES                  2124

#endif // RESOURCE_H
