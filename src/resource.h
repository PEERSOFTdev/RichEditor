#ifndef RESOURCE_H
#define RESOURCE_H

// Control IDs
#define IDC_RICHEDIT                    100
#define IDC_STATUSBAR                   101

// Menu resource
#define IDR_MENU_MAIN                   200
// Note: IDR_ACCELERATOR removed - now using dynamic accelerator table built in BuildAcceleratorTable()

// File menu
#define ID_FILE_NEW                     1001
#define ID_FILE_NEW_BLANK               1002  // New blank document (default)
#define ID_FILE_NEW_TEMPLATE_BASE       8000  // Base for File→New template items (8000-8031)
#define ID_FILE_OPEN                    1003
#define ID_FILE_SAVE                    1004
#define ID_FILE_SAVEAS                  1005
#define ID_FILE_READONLY                1006  // Toggle read-only mode
#define ID_FILE_EXIT                    1007

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
#define ID_VIEW_ZOOM_RESET              1111

// Search menu (Phase 2.9)
#define ID_SEARCH_FIND                  1300
#define ID_SEARCH_FIND_NEXT             1301
#define ID_SEARCH_FIND_PREVIOUS         1302
#define ID_SEARCH_REPLACE               1303
#define ID_SEARCH_GOTO_LINE             1304
#define ID_SEARCH_TOGGLE_BOOKMARK       1305
#define ID_SEARCH_NEXT_BOOKMARK         1306
#define ID_SEARCH_PREV_BOOKMARK         1307
#define ID_SEARCH_CLEAR_BOOKMARKS       1308

// Tools menu (Phase 2)
#define ID_TOOLS_EXECUTEFILTER          1201
#define ID_TOOLS_START_INTERACTIVE      1090  // Start Interactive Mode (Phase 2.5b)
#define ID_TOOLS_EXIT_INTERACTIVE       1091  // Exit Interactive Mode (Phase 2.5b)
#define ID_TOOLS_FILTER_BASE            5000  // Base for dynamic filter menu items (5000-5099)
#define ID_TOOLS_INSERT_TEMPLATE        1203  // Insert Template submenu
#define ID_TOOLS_SHOW_TEMPLATE_DESCRIPTIONS 1204  // Toggle template descriptions in menu
#define ID_TOOLS_TEMPLATE_BASE          7000  // Base for template menu items (7000-7099)

// File menu - MRU list
#define ID_FILE_MRU_BASE                6000  // Base for MRU menu items (6000-6009)

// Help menu
#define ID_HELP_ABOUT                   1901

// Dialog IDs
#define IDD_ABOUT                       300
#define IDD_FIND                        301
#define IDD_GOTO                        302
#define IDC_STATIC                      -1

// Find dialog control IDs (Phase 2.9)
#define IDC_FIND_WHAT                   3001
#define IDC_REPLACE_WITH                3002
#define IDC_REPLACE_WITH_LABEL          3003
#define IDC_MATCH_CASE                  3010
#define IDC_WHOLE_WORD                  3011
#define IDC_USE_ESCAPES                 3012
#define IDC_FIND_NEXT_BTN               3020
#define IDC_FIND_PREV_BTN               3021
#define IDC_REPLACE_BTN                 3022
#define IDC_REPLACE_ALL_BTN             3023
#define IDC_CLOSE_BTN                   3024

// Go to Line dialog control IDs (Phase 2.9.4)
#define IDC_GOTO_LINE                   3030
#define IDC_GOTO_LABEL                  3031

// String resource IDs (2000-2099)
#define IDS_ERROR                       2000
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
#define IDS_UNDO_REPLACE                2062  // Phase 2.9.2
#define IDS_REDO_REPLACE                2063  // Phase 2.9.2

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
#define IDS_RESUME_SAVE_PROMPT          2132  // Session recovery save prompt

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

// RichEdit library management strings (Phase 2.8: 2126-2130)
#define IDS_RICHEDIT_LOAD_FAILED        2126
#define IDS_RICHEDIT_VERSION            2128
#define IDS_RICHEDIT_CREATE_FAILED_DETAIL 2130

// Control IDs
#define IDC_RICHEDIT_VERSION            102
#define IDC_RICHEDIT_VERSION_PATH       103

// Search feature strings (Phase 2.9.1: 2140-2161)
#define IDS_FIND_TITLE                  2140
#define IDS_FIND_WHAT                   2141
#define IDS_MATCH_CASE                  2143
#define IDS_WHOLE_WORD                  2144
#define IDS_USE_ESCAPES                 2145
#define IDS_FIND_NEXT_BTN               2146
#define IDS_FIND_PREV_BTN               2147
#define IDS_FIND_NOTFOUND_PREFIX        2150
#define IDS_FIND_NOTFOUND_TITLE         2151

// Read-only mode
#define IDS_READONLY                    2152  // "Read-Only" / "Pouze ke čtení"

// Replace feature strings (Phase 2.9.2: 2153-2158)
#define IDS_FIND_REPLACE_TITLE          2153
#define IDS_REPLACE_WITH                2154
#define IDS_REPLACE_BTN                 2155
#define IDS_REPLACE_ALL_BTN             2156
#define IDS_REPLACE_COMPLETE_TITLE      2157
#define IDS_REPLACE_COMPLETE_MSG        2158

// Go to Line strings (Phase 2.9.4: 2159-2161)
#define IDS_GOTO_TITLE                  2159
#define IDS_GOTO_LABEL                  2160
#define IDS_GOTO_INVALID_LINE           2161

// Bookmark strings (Phase 2.9.3: 2162-2166)
#define IDS_BOOKMARK_TOGGLE             2162
#define IDS_BOOKMARK_NEXT               2163
#define IDS_BOOKMARK_PREV               2164
#define IDS_BOOKMARK_CLEAR_ALL          2165
#define IDS_NO_BOOKMARKS                2166

// Elevated save strings (ToDo #2)
#define IDS_ELEVATE_SAVE_PROMPT          2167
#define IDS_ERROR_ELEVATED_SAVE          2168

// URL auto-detection status (DetectURLs INI setting)
#define IDS_STATUS_AUTOURL_OFF          2169  // Status bar indicator when AURL disabled (DetectURLs=0)

#endif // RESOURCE_H
