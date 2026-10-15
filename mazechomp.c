/*
    MazeChomp - a tiny Pac-Man-style maze game for Windows CE / Embedded Compact
    Clean-room example using only Win32/WinCE GDI. No external assets or libraries.

    Controls:
      Arrow keys / D-pad
      Touch/click the on-screen arrows at the bottom
      R = restart
      Esc = quit

    Intended target: Windows Embedded Compact 7, ARM
*/

#define UNICODE
#define _UNICODE

#include <windows.h>

#define MAP_W 19
#define MAP_H 15
#define GHOSTS 3
#define TIMER_ID 1
#define TICK_MS 110

static const char *kMap[MAP_H] = {
    "###################",
    "#........#........#",
    "#.###.##.#.##.###.#",
    "#.................#",
    "#.###.#.#####.#.###",
    "#.....#...#...#...#",
    "#####.### # ###.###",
    "#.......     ......#",
    "###.#.### # ###.#.#",
    "#...#.....#.....#.#",
    "#.#####.#.#.#.#####",
    "#.........#.......#",
    "#.###.###.#.###.#.#",
    "#.................#",
    "###################"
};

typedef struct {
    int x, y;
    int dx, dy;
} Actor;

static HINSTANCE g_hInst;
static HWND g_hwnd;
static char g_map[MAP_H][MAP_W + 1];
static Actor g_player;
static Actor g_ghost[GHOSTS];
static int g_score = 0;
static int g_lives = 3;
static int g_gameOver = 0;
static unsigned long g_rng = 0x12345678UL;

static unsigned long prng(void)
{
    g_rng = g_rng * 1664525UL + 1013904223UL;
    return g_rng;
}

static int is_wall(int x, int y)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return 1;
    return g_map[y][x] == '#';
}

static void reset_positions(void)
{
    g_player.x = 9; g_player.y = 11;
    g_player.dx = -1; g_player.dy = 0;

    g_ghost[0].x = 8;  g_ghost[0].y = 7;
    g_ghost[1].x = 9;  g_ghost[1].y = 7;
    g_ghost[2].x = 10; g_ghost[2].y = 7;

    g_ghost[0].dx = 1;  g_ghost[0].dy = 0;
    g_ghost[1].dx = 0;  g_ghost[1].dy = 1;
    g_ghost[2].dx = -1; g_ghost[2].dy = 0;
}

static void new_game(void)
{
    int y, x;
    for (y = 0; y < MAP_H; ++y) {
        for (x = 0; x < MAP_W; ++x) {
            g_map[y][x] = kMap[y][x];
        }
        g_map[y][MAP_W] = 0;
    }

    /* Clear the central ghost box and player start. */
    g_map[7][8] = ' ';
    g_map[7][9] = ' ';
    g_map[7][10] = ' ';
    g_map[11][9] = ' ';

    g_score = 0;
    g_lives = 3;
    g_gameOver = 0;
    reset_positions();
    InvalidateRect(g_hwnd, NULL, TRUE);
}

static int can_move(const Actor *a, int dx, int dy)
{
    return !is_wall(a->x + dx, a->y + dy);
}

static void move_actor(Actor *a)
{
    int nx = a->x + a->dx;
    int ny = a->y + a->dy;

    if (!is_wall(nx, ny)) {
        a->x = nx;
        a->y = ny;
    }
}

static void set_player_dir(int dx, int dy)
{
    if (can_move(&g_player, dx, dy)) {
        g_player.dx = dx;
        g_player.dy = dy;
    }
}

static void ghost_ai(Actor *g)
{
    static const int dirs[4][2] = {
        {1,0}, {-1,0}, {0,1}, {0,-1}
    };
    int candidates[4];
    int count = 0;
    int i;
    int best = -1;
    int bestDist = 9999;

    for (i = 0; i < 4; ++i) {
        int dx = dirs[i][0], dy = dirs[i][1];
        int nx = g->x + dx, ny = g->y + dy;
        if (is_wall(nx, ny)) continue;

        /* Avoid reversing direction unless it is the only practical choice. */
        if (dx == -g->dx && dy == -g->dy) continue;

        candidates[count++] = i;

        /* Roughly chase player most of the time. */
        {
            int dist = (nx > g_player.x ? nx - g_player.x : g_player.x - nx)
                     + (ny > g_player.y ? ny - g_player.y : g_player.y - ny);
            if (dist < bestDist) {
                bestDist = dist;
                best = i;
            }
        }
    }

    if (count == 0) {
        g->dx = -g->dx;
        g->dy = -g->dy;
        return;
    }

    if ((prng() & 3UL) != 0 && best >= 0) {
        g->dx = dirs[best][0];
        g->dy = dirs[best][1];
    } else {
        int pick = candidates[prng() % count];
        g->dx = dirs[pick][0];
        g->dy = dirs[pick][1];
    }
}

static int same_cell(const Actor *a, const Actor *b)
{
    return a->x == b->x && a->y == b->y;
}

static int remaining_dots(void)
{
    int x, y, n = 0;
    for (y = 0; y < MAP_H; ++y)
        for (x = 0; x < MAP_W; ++x)
            if (g_map[y][x] == '.') ++n;
    return n;
}

static void game_tick(void)
{
    int i;

    if (g_gameOver) return;

    move_actor(&g_player);

    if (g_map[g_player.y][g_player.x] == '.') {
        g_map[g_player.y][g_player.x] = ' ';
        g_score += 10;
    }

    for (i = 0; i < GHOSTS; ++i) {
        ghost_ai(&g_ghost[i]);
        move_actor(&g_ghost[i]);
    }

    for (i = 0; i < GHOSTS; ++i) {
        if (same_cell(&g_player, &g_ghost[i])) {
            --g_lives;
            if (g_lives <= 0) {
                g_gameOver = 1;
            } else {
                reset_positions();
            }
            break;
        }
    }

    if (!g_gameOver && remaining_dots() == 0) {
        g_gameOver = 2; /* win */
    }

    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void draw_centered_text(HDC hdc, RECT rc, LPCWSTR text)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255,255,255));
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void paint_game(HWND hwnd, HDC hdc)
{
    RECT rc;
    RECT statusRc, boardRc, controlsRc;
    int clientW, clientH;
    int cellW, cellH, cell;
    int boardW, boardH;
    int ox, oy;
    int x, y, i;
    WCHAR buf[128];
    HBRUSH black = CreateSolidBrush(RGB(0,0,0));
    HBRUSH wall = CreateSolidBrush(RGB(20,80,220));
    HBRUSH pellet = CreateSolidBrush(RGB(245,245,245));
    HBRUSH player = CreateSolidBrush(RGB(255,220,0));
    HBRUSH ghostBrush[GHOSTS];
    HBRUSH controlBrush = CreateSolidBrush(RGB(70,70,70));

    ghostBrush[0] = CreateSolidBrush(RGB(255,60,60));
    ghostBrush[1] = CreateSolidBrush(RGB(255,140,210));
    ghostBrush[2] = CreateSolidBrush(RGB(70,220,255));

    GetClientRect(hwnd, &rc);
    clientW = rc.right - rc.left;
    clientH = rc.bottom - rc.top;

    FillRect(hdc, &rc, black);

    statusRc.left = 0;
    statusRc.top = 0;
    statusRc.right = clientW;
    statusRc.bottom = 28;

    controlsRc.left = 0;
    controlsRc.right = clientW;
    controlsRc.bottom = clientH;
    controlsRc.top = clientH - 70;

    boardRc.left = 0;
    boardRc.top = statusRc.bottom;
    boardRc.right = clientW;
    boardRc.bottom = controlsRc.top;

    wsprintfW(buf, L"MazeChomp    Score: %d    Lives: %d", g_score, g_lives);
    draw_centered_text(hdc, statusRc, buf);

    cellW = (boardRc.right - boardRc.left) / MAP_W;
    cellH = (boardRc.bottom - boardRc.top) / MAP_H;
    cell = cellW < cellH ? cellW : cellH;
    if (cell < 4) cell = 4;

    boardW = cell * MAP_W;
    boardH = cell * MAP_H;
    ox = (clientW - boardW) / 2;
    oy = boardRc.top + ((boardRc.bottom - boardRc.top) - boardH) / 2;

    for (y = 0; y < MAP_H; ++y) {
        for (x = 0; x < MAP_W; ++x) {
            RECT r;
            r.left = ox + x * cell;
            r.top = oy + y * cell;
            r.right = r.left + cell;
            r.bottom = r.top + cell;

            if (g_map[y][x] == '#') {
                RECT in = r;
                InflateRect(&in, -1, -1);
                FillRect(hdc, &in, wall);
            } else if (g_map[y][x] == '.') {
                int d = cell / 5;
                if (d < 2) d = 2;
                Ellipse(hdc,
                        r.left + cell/2 - d/2,
                        r.top + cell/2 - d/2,
                        r.left + cell/2 + d/2 + 1,
                        r.top + cell/2 + d/2 + 1);
            }
        }
    }

    /* Player */
    {
        int l = ox + g_player.x * cell + 2;
        int t = oy + g_player.y * cell + 2;
        int r = l + cell - 4;
        int b = t + cell - 4;
        HBRUSH old = (HBRUSH)SelectObject(hdc, player);
        Ellipse(hdc, l, t, r, b);
        SelectObject(hdc, old);
    }

    /* Ghosts */
    for (i = 0; i < GHOSTS; ++i) {
        int l = ox + g_ghost[i].x * cell + 2;
        int t = oy + g_ghost[i].y * cell + 2;
        int r = l + cell - 4;
        int b = t + cell - 4;
        HBRUSH old = (HBRUSH)SelectObject(hdc, ghostBrush[i]);
        Ellipse(hdc, l, t, r, b);
        SelectObject(hdc, old);
    }

    /* Touch controls: four equal buttons */
    {
        RECT b[4];
        LPCWSTR labels[4] = { L"<", L"^", L"v", L">" };
        int j;
        int w = clientW / 4;
        for (j = 0; j < 4; ++j) {
            b[j].left = j * w + 2;
            b[j].right = (j == 3 ? clientW : (j+1)*w) - 2;
            b[j].top = controlsRc.top + 4;
            b[j].bottom = controlsRc.bottom - 4;
            FillRect(hdc, &b[j], controlBrush);
            draw_centered_text(hdc, b[j], labels[j]);
        }
    }

    if (g_gameOver) {
        RECT msg = rc;
        msg.top = clientH / 3;
        msg.bottom = (clientH * 2) / 3;
        FillRect(hdc, &msg, black);
        if (g_gameOver == 2)
            draw_centered_text(hdc, msg, L"YOU WIN!  Press R to restart");
        else
            draw_centered_text(hdc, msg, L"GAME OVER  Press R to restart");
    }

    DeleteObject(controlBrush);
    DeleteObject(player);
    DeleteObject(pellet);
    DeleteObject(wall);
    DeleteObject(black);
    for (i = 0; i < GHOSTS; ++i) DeleteObject(ghostBrush[i]);
}

static void handle_touch(HWND hwnd, int x, int y)
{
    RECT rc;
    int w;
    GetClientRect(hwnd, &rc);

    if (y < rc.bottom - 70) return;

    w = (rc.right - rc.left) / 4;
    if (x < w) set_player_dir(-1, 0);
    else if (x < w * 2) set_player_dir(0, -1);
    else if (x < w * 3) set_player_dir(0, 1);
    else set_player_dir(1, 0);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, TIMER_ID, TICK_MS, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID) game_tick();
        return 0;

    case WM_KEYDOWN:
        switch (wParam) {
        case VK_LEFT:  set_player_dir(-1, 0); break;
        case VK_RIGHT: set_player_dir(1, 0); break;
        case VK_UP:    set_player_dir(0, -1); break;
        case VK_DOWN:  set_player_dir(0, 1); break;
        case 'R':      new_game(); break;
        case VK_ESCAPE: DestroyWindow(hwnd); break;
        }
        return 0;

    case WM_LBUTTONDOWN:
        handle_touch(hwnd, LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paint_game(hwnd, hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPWSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSW wc;
    MSG msg;
    RECT r;
    int sw, sh;

    (void)hPrevInstance;
    (void)lpCmdLine;

    g_hInst = hInstance;

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"MazeChompWinCE";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassW(&wc))
        return 1;

    sw = GetSystemMetrics(SM_CXSCREEN);
    sh = GetSystemMetrics(SM_CYSCREEN);

    r.left = 0;
    r.top = 0;
    r.right = sw;
    r.bottom = sh;

    g_hwnd = CreateWindowW(
        L"MazeChompWinCE",
        L"MazeChomp",
        WS_VISIBLE,
        0, 0, sw, sh,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hwnd)
        return 2;

    new_game();
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}