#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

using namespace std;

const int BOARD_WIDTH = 10;
const int BOARD_HEIGHT = 20;
const int PREVIEW_SIZE = 4;

// ANSI Farb-Codes für Linux Terminal
const string RESET = "\033[0m";
const string BLACK = "\033[30m";
const string RED = "\033[31m";
const string GREEN = "\033[32m";
const string YELLOW = "\033[33m";
const string BLUE = "\033[34m";
const string MAGENTA = "\033[35m";
const string CYAN = "\033[36m";
const string WHITE = "\033[37m";
const string BRIGHT_BLACK = "\033[90m";
const string BRIGHT_RED = "\033[91m";
const string BRIGHT_GREEN = "\033[92m";
const string BRIGHT_YELLOW = "\033[93m";
const string BRIGHT_BLUE = "\033[94m";
const string BRIGHT_MAGENTA = "\033[95m";
const string BRIGHT_CYAN = "\033[96m";
const string BRIGHT_WHITE = "\033[97m";

// Tetris Steine (Tetrominoes)
vector<vector<vector<int>>> tetrominoes = {
    // I-Stück (Cyan)
    {{{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}}},
    // O-Stück (Gelb)
    {{{0,0,0,0}, {0,2,2,0}, {0,2,2,0}, {0,0,0,0}}},
    // T-Stück (Lila)
    {{{0,0,0,0}, {0,3,0,0}, {3,3,3,0}, {0,0,0,0}}},
    // S-Stück (Grün)
    {{{0,0,0,0}, {0,4,4,0}, {4,4,0,0}, {0,0,0,0}}},
    // Z-Stück (Rot)
    {{{0,0,0,0}, {5,5,0,0}, {0,5,5,0}, {0,0,0,0}}},
    // J-Stück (Blau)
    {{{0,0,0,0}, {6,0,0,0}, {6,6,6,0}, {0,0,0,0}}},
    // L-Stück (Orange)
    {{{0,0,0,0}, {0,0,7,0}, {7,7,7,0}, {0,0,0,0}}}
};

// Farben für die Steine
vector<string> pieceColors = {
    WHITE, BRIGHT_CYAN, BRIGHT_YELLOW, BRIGHT_MAGENTA, 
    BRIGHT_GREEN, BRIGHT_RED, BRIGHT_BLUE, YELLOW
};

class Tetris {
private:
    vector<vector<int>> board;
    vector<vector<int>> currentPiece;
    int currentX, currentY;
    int currentType;
    int nextType;
    int score;
    int level;
    int linesCleared;
    bool gameOver;
    struct termios oldTermios;

    void setupTerminal() {
        struct termios newTermios;
        tcgetattr(STDIN_FILENO, &oldTermios);
        newTermios = oldTermios;
        newTermios.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    }

    void restoreTerminal() {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
    }

    void clearScreen() {
        cout << "\033[2J\033[H";
    }

    void gotoxy(int x, int y) {
        cout << "\033[" << y + 1 << ";" << x + 1 << "H";
    }

    void hideCursor() {
        cout << "\033[?25l";
    }

    void showCursor() {
        cout << "\033[?25h";
    }

    int kbhit() {
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        return select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout) > 0;
    }

    char getch() {
        return getchar();
    }

    vector<vector<int>> rotatePiece(const vector<vector<int>>& piece) {
        int n = piece.size();
        vector<vector<int>> rotated(n, vector<int>(n));
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                rotated[j][n-1-i] = piece[i][j];
            }
        }
        return rotated;
    }

    bool isValidPosition(const vector<vector<int>>& piece, int x, int y) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (piece[i][j] != 0) {
                    int newX = x + j;
                    int newY = y + i;
                    if (newX < 0 || newX >= BOARD_WIDTH || 
                        newY >= BOARD_HEIGHT || 
                        (newY >= 0 && board[newY][newX] != 0)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    void placePiece() {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (currentPiece[i][j] != 0) {
                    int x = currentX + j;
                    int y = currentY + i;
                    if (y >= 0 && y < BOARD_HEIGHT && x >= 0 && x < BOARD_WIDTH) {
                        board[y][x] = currentPiece[i][j];
                    }
                }
            }
        }
    }

    void clearLines() {
        int cleared = 0;
        for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
            bool fullLine = true;
            for (int x = 0; x < BOARD_WIDTH; x++) {
                if (board[y][x] == 0) {
                    fullLine = false;
                    break;
                }
            }
            if (fullLine) {
                board.erase(board.begin() + y);
                board.insert(board.begin(), vector<int>(BOARD_WIDTH, 0));
                cleared++;
                y++; // Überprüfe die gleiche Zeile nochmal
            }
        }
        
        if (cleared > 0) {
            linesCleared += cleared;
            score += cleared * cleared * 100 * (level + 1);
            level = linesCleared / 10;
        }
    }

    void spawnPiece() {
        currentType = nextType;
        nextType = rand() % tetrominoes.size();
        currentPiece = tetrominoes[currentType];
        currentX = BOARD_WIDTH / 2 - 2;
        currentY = 0;

        if (!isValidPosition(currentPiece, currentX, currentY)) {
            gameOver = true;
        }
    }

    void drawBoard() {
        // Erstelle temporäres Display-Board
        vector<vector<int>> displayBoard = board;
        
        // Füge Ghost-Stück hinzu
        int ghostY = currentY;
        while (isValidPosition(currentPiece, currentX, ghostY + 1)) {
            ghostY++;
        }
        
        // Zeichne Ghost nur wenn es sich vom aktuellen Stück unterscheidet
        if (ghostY != currentY) {
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    if (currentPiece[i][j] != 0) {
                        int x = currentX + j;
                        int y = ghostY + i;
                        if (y >= 0 && y < BOARD_HEIGHT && x >= 0 && x < BOARD_WIDTH && displayBoard[y][x] == 0) {
                            displayBoard[y][x] = -1; // Special ghost marker
                        }
                    }
                }
            }
        }

        // Füge aktuelles Stück hinzu
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (currentPiece[i][j] != 0) {
                    int x = currentX + j;
                    int y = currentY + i;
                    if (y >= 0 && y < BOARD_HEIGHT && x >= 0 && x < BOARD_WIDTH) {
                        displayBoard[y][x] = currentPiece[i][j];
                    }
                }
            }
        }

        // Zeichne oberen Rahmen
        gotoxy(1, 1);
        cout << WHITE << "╔";
        for (int i = 0; i < BOARD_WIDTH * 2; i++) cout << "═";
        cout << "╗" << RESET;

        // Zeichne Seiten-Rahmen und Inhalt
        for (int y = 0; y < BOARD_HEIGHT; y++) {
            // Linker Rahmen
            gotoxy(1, y + 2);
            cout << WHITE << "║" << RESET;
            
            // Board-Inhalt
            for (int x = 0; x < BOARD_WIDTH; x++) {
                gotoxy(x * 2 + 2, y + 2);
                if (displayBoard[y][x] == 0) {
                    cout << "  ";
                } else if (displayBoard[y][x] == -1) {
                    // Ghost-Stück
                    cout << BRIGHT_BLACK << "░░" << RESET;
                } else {
                    // Normales Stück
                    cout << pieceColors[displayBoard[y][x]] << "██" << RESET;
                }
            }
            
            // Rechter Rahmen
            gotoxy(BOARD_WIDTH * 2 + 2, y + 2);
            cout << WHITE << "║" << RESET;
        }

        // Zeichne unteren Rahmen
        gotoxy(1, BOARD_HEIGHT + 2);
        cout << WHITE << "╚";
        for (int i = 0; i < BOARD_WIDTH * 2; i++) cout << "═";
        cout << "╝" << RESET;
    }

    void drawUI() {
        // Score und Level
        gotoxy(BOARD_WIDTH * 2 + 5, 2);
        cout << WHITE << "Score: " << BRIGHT_YELLOW << score << RESET;
        gotoxy(BOARD_WIDTH * 2 + 5, 3);
        cout << WHITE << "Level: " << BRIGHT_YELLOW << level << RESET;
        gotoxy(BOARD_WIDTH * 2 + 5, 4);
        cout << WHITE << "Lines: " << BRIGHT_YELLOW << linesCleared << RESET;

        // Nächstes Stück
        gotoxy(BOARD_WIDTH * 2 + 5, 6);
        cout << WHITE << "Next:" << RESET;
        
        auto nextPiece = tetrominoes[nextType];
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                gotoxy(BOARD_WIDTH * 2 + 5 + j * 2, 7 + i);
                if (nextPiece[i][j] != 0) {
                    cout << pieceColors[nextPiece[i][j]] << "██" << RESET;
                } else {
                    cout << "  ";
                }
            }
        }

        // Steuerung
        gotoxy(BOARD_WIDTH * 2 + 5, 12);
        cout << WHITE << "Controls:" << RESET;
        gotoxy(BOARD_WIDTH * 2 + 5, 13);
        cout << "A/D - Move";
        gotoxy(BOARD_WIDTH * 2 + 5, 14);
        cout << "S - Soft Drop";
        gotoxy(BOARD_WIDTH * 2 + 5, 15);
        cout << "W - Rotate";
        gotoxy(BOARD_WIDTH * 2 + 5, 16);
        cout << "Q - Quit";
    }

    void handleInput() {
        if (kbhit()) {
            char key = getch();
            
            switch (key) {
                case 'a':
                case 'A':
                    if (isValidPosition(currentPiece, currentX - 1, currentY)) {
                        currentX--;
                    }
                    break;
                case 'd':
                case 'D':
                    if (isValidPosition(currentPiece, currentX + 1, currentY)) {
                        currentX++;
                    }
                    break;
                case 's':
                case 'S':
                    if (isValidPosition(currentPiece, currentX, currentY + 1)) {
                        currentY++;
                        score += 1;
                    }
                    break;
                case 'w':
                case 'W':
                    {
                        auto rotated = rotatePiece(currentPiece);
                        if (isValidPosition(rotated, currentX, currentY)) {
                            currentPiece = rotated;
                        }
                    }
                    break;
                case 'q':
                case 'Q':
                    gameOver = true;
                    break;
            }
        }
    }

    bool shouldPieceFall() {
        static auto lastFall = chrono::steady_clock::now();
        auto now = chrono::steady_clock::now();
        int fallDelay = max(100, 800 - level * 80); // Langsamerer Start, bessere Progression
        
        if (chrono::duration_cast<chrono::milliseconds>(now - lastFall).count() >= fallDelay) {
            lastFall = now;
            return true;
        }
        return false;
    }

public:
    Tetris() : board(BOARD_HEIGHT, vector<int>(BOARD_WIDTH, 0)), 
               currentX(0), currentY(0), currentType(0), score(0), 
               level(0), linesCleared(0), gameOver(false) {
        srand(time(nullptr));
        nextType = rand() % tetrominoes.size();
        setupTerminal();
        hideCursor();
        clearScreen();
    }

    ~Tetris() {
        showCursor();
        restoreTerminal();
    }

    void run() {
        spawnPiece();
        
        while (!gameOver) {
            clearScreen();
            drawBoard();
            drawUI();
            
            handleInput();
            
            if (shouldPieceFall()) {
                if (isValidPosition(currentPiece, currentX, currentY + 1)) {
                    currentY++;
                } else {
                    placePiece();
                    clearLines();
                    spawnPiece();
                }
            }
            
            cout.flush();
            this_thread::sleep_for(chrono::milliseconds(16)); // ~60 FPS
        }
        
        // Game Over Screen
        clearScreen();
        gotoxy(10, 10);
        cout << BRIGHT_RED << "GAME OVER!" << RESET << endl;
        gotoxy(10, 11);
        cout << WHITE << "Final Score: " << BRIGHT_YELLOW << score << RESET << endl;
        gotoxy(10, 12);
        cout << WHITE << "Press any key to exit..." << RESET << endl;
        
        // Warte auf Eingabe
        fcntl(STDIN_FILENO, F_SETFL, 0); // Blocking mode
        getchar();
    }
};

int main() {
    cout << BRIGHT_CYAN << "=== TERMINAL TETRIS ===" << RESET << endl;
    cout << "Press any key to start..." << endl;
    cin.get();
    
    Tetris game;
    game.run();
    
    return 0;
} 
