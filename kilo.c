/* Includes */
#include <ctype.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/* Data */
struct editorConfig
{
    int screenRows;
    int screenCols;
    struct termios orig_termios;
};

struct editorConfig E;

/* Defines*/
#define CTRL_KEY(k) ((k) & 0x1f)

/* Termninal related functions */
void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr-disablefn");
}

void enableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr-enablefn");
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    raw.c_iflag = ~(IXON | ICRNL | BRKINT | ISTRIP | INPCK);
    raw.c_oflag = ~(OPOST);
    raw.c_cflag = ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr-enableRaw");
}

char editorReadKey()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    return c;
}

int getCursorPosition(int *rows, int *cols)
{
    char buff[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buff))
    {
        if (read(STDIN_FILENO, &buff[i], 1) != 1)
            break;
        if (buff[i] == 'R')
            break;
        i++;
    }
    buff[i] = '\0';
    if (buff[0] != '\x1b' || buff[1] != '[')
        return -1;
    if (sscanf(&buff[2], "%d;%d", rows, cols) != 2)
        return -1;
    editorReadKey();
    return -1;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0)
    {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        editorReadKey();
        return getCursorPosition(rows, cols);
    }
    else
    {
        *cols = w.ws_col;
        *rows = w.ws_row;
        return 0;
    }
}

/* Handles Input */
void editorProcessKeyPress()
{
    char c = editorReadKey();
    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}

/* Append Buffer */
struct abuf
{
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, char *s, int length)
{
    char *new = realloc(ab->b, ab->len + length);
    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, length);
    ab->b = new;
    ab->len += length;
}

void abFree(struct abuf *ab)
{
    free(ab->b);
}

/* Handles Output */
void editorDrawRows(struct abuf *ab)
{
    for (int i = 0; i < E.screenRows; i++)
    {
        abAppend(ab, "~", 1);
        abAppend(ab, "\x1b[K", 3);
        if (i < E.screenRows - 1)
        {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen()
{
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDIN_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/* Init */
void initEditor()
{
    if (getWindowSize(&E.screenRows, &E.screenCols) == -1)
        die("initEditor");
}

int main()
{
    enableRawMode();
    initEditor();
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeyPress();
    }
    return 0;
}