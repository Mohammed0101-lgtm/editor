#include <termios.h>
#include <ncurses.h>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <cassert>
#include <string>
#include <vector>
// #include "highlight.h"

#define TAB_STOP 4

static int syntax_mode;

enum : int {
    NORMAL,
    C
};

struct Row {
    int         index = 0;
    std::string characters;
    std::string printed;
    int         highlight_open_comment;
};


struct Editor {
    std::string              filename;
    size_t                   number_of_rows;
    int                      cursor_x;
    int                      cursor_y;
    std::vector<std::string> screen_rows;
    std::vector<struct Row>  rows;
};


struct Editor edit;

// prototypes

std::string rows_to_string();
void        init();
void        delete_char();
void        process_key();
void        endTerminal();
void        initTerminal();
void        update_syntax();
void        refresh_screen();
void        insert_newline();
void        insert_char(int c);
void        delete_row(int pos);
void        move_cursor(int key);
void        update_row(struct Row* row);
void        select_syntax_highlighting();
void        load_file(const std::string& filename);
void        insert_row(int pos, const std::string& str);
void        delete_char_in_row(struct Row* row, int pos);
void        add_string_to_row(struct Row* row, std::string str);
void        insert_char_to_row(struct Row* row, int pos, char c);


int main(int argc, char** argv) {
    init();
    initTerminal();

    if (argc >= 2)
    {
        load_file(argv[1]);
        select_syntax_highlighting();
    }
    else
    {
        syntax_mode = NORMAL;
    }

    while (true)
    {
        refresh_screen();
        process_key();
    }

    endTerminal();
    return 0;
}

/*--Terminal stuff--*/

void initTerminal() {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    scrollok(stdscr, TRUE);
}


void endTerminal() { endwin(); }


void kill_editor(std::string error_message) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(error_message.c_str());
    exit(1);
}


void refresh_screen() {
    clear();

    int height, width;
    getmaxyx(stdscr, height, width);

    int current_row = 0;
    int current_col = 0;

    for (int i = 0; i < edit.number_of_rows; i++)
    {
        std::string row = edit.rows[i].printed;

        for (char c : row)
        {
            printw("%c", c);

            current_col++;

            if (current_col >= width)
            {
                current_col = 0;
                current_row++;

                if (current_row >= height)
                {
                    current_row = 0;
                    // Optionally, handle scrolling or other logic here
                }

                move(current_row, current_col);
            }
        }

        current_row++;
        current_col = 0;

        if (current_row >= height)
        {
            current_row = 0;
            // Optionally, handle scrolling or other logic here
        }

        move(current_row, current_col);
    }

    move(edit.cursor_y, edit.cursor_x);
}


void move_cursor(int key) {
    struct Row* row = (edit.cursor_y >= edit.number_of_rows) ? nullptr : &edit.rows[edit.cursor_y];

    switch (key)
    {
    case KEY_LEFT :
        if (edit.cursor_x != 0)
        {
            edit.cursor_x--;
        }
        else if (edit.cursor_y > 0)
        {
            edit.cursor_y--;
            edit.cursor_x = edit.rows[edit.cursor_y].printed.size();
        }

        break;
    case KEY_RIGHT :
        if (row && edit.cursor_x < row->characters.size())
        {
            edit.cursor_x++;
        }
        else if (row && edit.cursor_x == row->printed.size())
        {
            edit.cursor_y++;
            edit.cursor_x = 0;
        }

        break;
    case KEY_UP :
        if (edit.cursor_y != 0)
        {
            edit.cursor_y--;
        }

        break;
    case KEY_DOWN :
        if (edit.cursor_y < edit.number_of_rows)
        {
            edit.cursor_y++;
        }

        break;
    }

    row        = (edit.cursor_y >= edit.number_of_rows) ? NULL : &edit.rows[edit.cursor_y];
    int rowlen = row ? row->printed.size() : 0;

    if (edit.cursor_x > rowlen)
    {
        edit.cursor_x = rowlen;
    }
}


void process_key() {
    int c = getch();

    switch (c)
    {
    case '\n' :
        insert_newline();
        break;
    case '\t' :
        edit.rows[edit.cursor_y].characters.insert(edit.cursor_x, "\t");
        break;
    case 127 :
        delete_char();
        move_cursor(KEY_LEFT);
        break;
    case KEY_HOME :
        edit.cursor_x = 0;
        break;
    case KEY_SAVE :
        // save_file();
        break;
    case KEY_UP :
    case KEY_DOWN :
    case KEY_LEFT :
    case KEY_RIGHT :
        move_cursor(c);
        break;
    default :
        insert_char(c);
    }
}


void init() {
    edit.cursor_x       = 0;
    edit.cursor_y       = 0;
    edit.number_of_rows = 0;
    edit.filename       = "";
    // highlight_init(NULL);
}


/*--File operations--*/
void load_file(const std::string& filename) {
    edit.filename.clear();
    edit.filename = filename;

    if (edit.filename.empty())
    {
        return;
    }

    std::ifstream file(edit.filename, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "Failed to open file" << std::endl;
        return;
    }

    edit.rows.clear();

    Row  current_row;
    char c;

    while (file.get(c))
    {
        if (c == '\n')
        {
            if (!current_row.characters.empty())
            {
                edit.rows.push_back(current_row);
                current_row.characters.clear();
            }
        }
        else
        {
            current_row.characters.push_back(c);
        }
    }

    if (!current_row.characters.empty())
    {
        edit.rows.push_back(current_row);
    }

    file.close();
}


void save_file() {
    if (edit.filename.empty())
    {
        return;
    }

    std::ofstream file(edit.filename, std::ios::app | std::ios::out);
    if (!file.is_open())
    {
        std::cout << "Failed to save file" << std::endl;
        return;
    }

    std::string ouput = rows_to_string();
    file << ouput;
    file.close();
}

/*--Text level stuff--*/

void delete_char() {
    if (edit.cursor_x == 0 && edit.cursor_y == 0)
    {
        return;
    }

    struct Row* row = &edit.rows[edit.cursor_y];

    if (edit.cursor_x > 0)
    {
        delete_char_in_row(row, edit.cursor_x - 1);
    }
    else if (edit.cursor_x == 0)
    {
        struct Row* prev_row = &edit.rows[edit.cursor_y - 1];
        prev_row->characters += row->characters;
        delete_row(edit.cursor_y);
    }
}


void insert_newline() {
    struct Row* current_row = &edit.rows[edit.cursor_y];
    if (current_row == nullptr)
    {
        return;
    }

    size_t row_len = current_row->characters.length();

    if (edit.cursor_x == row_len)
    {
        insert_row(edit.cursor_y + 1, "");
        move_cursor(KEY_DOWN);
    }
    else if (edit.cursor_x == 0)
    {
        insert_row(edit.cursor_y, "");
    }
    else
    {
        std::string left_over = current_row->characters.substr(edit.cursor_x + 1, row_len);
        current_row->characters[edit.cursor_x] = '\0';
        insert_row(edit.cursor_y + 1, "");
        add_string_to_row(&edit.rows[edit.cursor_y + 1], left_over);
    }
}


void insert_char(int c) {
    if (edit.cursor_y == edit.number_of_rows)
    {
        insert_row(edit.number_of_rows, "");
    }

    insert_char_to_row(&edit.rows[edit.cursor_y], edit.cursor_x, c);
    edit.cursor_x++;
}


void select_syntax_highlighting() {
    size_t pos_ext = edit.filename.find_last_of('.');
    if (pos_ext == std::string::npos || pos_ext == edit.filename.length() - 1)
    {
        syntax_mode = NORMAL;
        return;
    }

    std::string ext = edit.filename.substr(pos_ext + 1);
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    syntax_mode = ext == "c" ? C : NORMAL;
}


int is_separator(int c) { return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL; }


void update_syntax() {
    struct Row* row;

    for (struct Row row : edit.rows)
    {
        std::string str         = row.printed;
        char*       highlighted = NULL;  // = highlight_line(str.c_str(), NULL, str.size());
        if (highlighted == NULL)
            return;
        row.printed = highlighted;
    }
}


/*--row operations--*/


void insert_row(int pos, const std::string& str) {
    if (pos < 0 || pos > edit.number_of_rows)
    {
        return;
    }

    edit.rows.insert(edit.rows.begin() + pos, Row());

    for (int i = pos + 1; i < edit.number_of_rows + 1; ++i)
    {
        edit.rows[i].index++;
    }

    edit.rows[pos].index      = pos;
    edit.rows[pos].characters = str;

    update_row(&edit.rows[pos]);
    edit.number_of_rows++;
}


void update_row(Row* row) {
    int    tabs = 0;
    size_t len  = row->characters.length();
    for (size_t i = 0; i < len; i++)
    {
        if (row->characters[i] == '\t')
        {
            tabs++;
        }
    }

    row->printed.resize(len + tabs * (TAB_STOP - 1));

    int index = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (row->characters[i] == '\t')
        {
            int spaces_to_add = TAB_STOP - (index % TAB_STOP);
            row->printed.replace(index, spaces_to_add, spaces_to_add, ' ');
            index += spaces_to_add;
        }
        else
        {
            row->printed[index++] = row->characters[i];
        }
    }

    row->printed.resize(index);
}


void insert_char_to_row(Row* row, int pos, char c) {
    int rlen = static_cast<int>(row->characters.length());

    if (pos < 0 || pos > rlen)
    {
        pos = rlen;
    }

    row->characters.insert(pos, 1, c);
    update_row(row);
}


void delete_char_in_row(struct Row* row, int pos) {
    size_t rlen = row->characters.length();
    if (pos < 0 || pos >= static_cast<int>(rlen))
    {
        return;
    }

    row->characters.erase(pos, 1);
    update_row(row);
}


void delete_row(int pos) {
    if (pos < 0 || pos >= edit.number_of_rows)
    {
        return;
    }

    edit.rows.erase(edit.rows.begin() + pos);
    edit.number_of_rows--;

    for (size_t i = pos; i < edit.number_of_rows; ++i)
    {
        edit.rows[i].index--;
    }
}


void add_string_to_row(struct Row* row, std::string str) {
    size_t rlen = row->characters.length();
    row->characters.resize(rlen + str.length() + 1);
    row->characters[rlen] = '\0';
    update_row(row);
}


std::string rows_to_string() {
    std::string str;

    for (int i = 0; i < edit.number_of_rows; i++)
    {
        std::string row_string = edit.rows[i].characters;
        str += row_string;
    }

    return str;
}
