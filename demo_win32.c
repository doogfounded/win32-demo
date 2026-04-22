//© doogfounded
//This is ment to be a simple demo of Win32 that should work across every platform that supports Win32 (eg 9x, NT, CE, ReactOS, Wine all both 32 and 64 bit versions of them)
/*Todo list:
MANDATORY:
1. The "Controls" menu exists and I can click on stuff but nothing happens. Find a fix for this.
2. One of the sliders at the bottom doesn't do anything. Remove it or figure out another use.
Nice to have/future goals:
1. Give the user a larger amount of control over things eg colors, 
toggling bouncing behavior 
(including making them never bounce and having them come around after going off screen), 
etc. Maybe add some checkboxes for toggling different visual 
effects on the sprites or something.
2. Add a menu that allows the user to change the number of sprites (up to a reasonable limit like 10 or 20) 
and maybe also the size of the sprites. 
This would require dynamically allocating the sprite array 
and adding some UI for inputting these values, 
but it would make the demo more interactive 
and allow users to see how the performance changes with more sprites on screen. Maybe a menu on startup that asks 
for these values and then creates the window with the appropriate size and number of sprites 
based on user input.
3. Add a real pause menu with UI instead of just a text overlay. 
This would involve creating a new window or dialog that appears 
when the game is paused, with options to resume, adjust settings, or exit. 
It would make the pause functionality more user-friendly and visually appealing.
4. Have the user be able to change the background of the window to a custom image loaded from disk.
5. Have a startup video the plays before the main menu, maybe with some cool animations and music. 
6. Add sound effects when the sprites bounce off the walls, and maybe some background music as well.
7. Have an about box with info about the project, the author, build date, and info about how to play.
8. Have an in game manual that explains the controls and features of the demo, maybe with some fun illustrations and tips for getting the most out of it.
One day this would maybe be some kind of real game in a real yet simple engine that works across almost every single version of Win32 and different ISAs with mechanics levels story maybe multiplayer etc with sprites being actual things and more.
*/
#include <windows.h>
#include <stdio.h>
#include <commctrl.h>
#include <stdlib.h>

// ---------------------------
// Game constants
// ---------------------------
#define WINDOW_WIDTH  320
#define WINDOW_HEIGHT 240
#define SPRITE_SIZE   32
#define NUM_FRAMES    4
#define NUM_SPRITES   3
#define IDM_SHOW_CONTROLS 2001
#define IDM_HIDE_CONTROLS 2002
// ---------------------------
// Game constants (mutable for UI)
// ---------------------------
int SPEED = 4;
int ANIMATION_TICK = 8;

// ---------------------------
// Control IDs
// ---------------------------
#define ID_SLIDER_ANIM  1002
#define ID_EDIT_ANIM    1004
// Added: speed slider
#define ID_SLIDER_SPEED 1001

// ---------------------------
// Global game state
// ---------------------------
int currentFrame = 0;
int animationTick = 0;
DWORD lastFrameTime = 0;
DWORD startTime = 0; // program start timestamp in ms
int fps = 0;
int fpsFrameCount = 0;
DWORD fpsLastUpdate = 0;
int isPaused = 0; // 0 = running, 1 = paused
// Global variables for the control dialog
HWND hControlDialog = NULL;
HWND hSliderAnimDlg = NULL;
HWND hEditAnimDlg = NULL;
// Handles for sliders referenced elsewhere
HWND hSliderSpeed = NULL;
HWND hSliderAnim = NULL;

// Table for sprite management
typedef struct {
    int x;
    int y;
    int velocityX;
    int velocityY;
} Sprite;

Sprite sprites[3] = {
    { 50,  50,  4, 4 },
    { 150, 120, 4, 4 },
    { 2,   5,   4, 4 }
};


// ---------------------------
// Backbuffer objects (reused)
// ---------------------------
HDC hdcBackBuffer = NULL;
HBITMAP hbmBackBuffer = NULL;
HBITMAP hbmOld = NULL;

// ---------------------------
// Forward declarations
// ---------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void update_game();
void render_game(HDC hdc);
void draw_sprite(HDC hdc, int x, int y, int frame);
void init_backbuffer(HWND hwnd);
void cleanup_backbuffer();
// Dialog procedure for the floating control panel
LRESULT CALLBACK DialogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_DESTROY:
            // Instead of destroying, just hide the window so controls remain
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        
        case WM_VSCROLL:
{
    int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
    HWND ctrl = (HWND)lParam;
    if (ctrl == hSliderAnim || ctrl == hSliderAnimDlg) {
        ANIMATION_TICK = pos;
        char buf[16];
        sprintf(buf, "%d", pos);
        SetWindowText(hEditAnimDlg, buf);
    }
    else if (ctrl == hSliderSpeed) {
        SPEED = pos;
    }
    return 0;
}

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                ShowWindow(hwnd, SW_HIDE);
            }
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}
void CreateControlDialog(HINSTANCE hInstance)
{
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = DialogWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ControlDialogClass";
    RegisterClassA(&wc);

    hControlDialog = CreateWindowExA(
        WS_EX_TOPMOST, // Float above other windows
        "ControlDialogClass",
        "Speed of Animation Control Pannel",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        400, 200, // X, Y position on screen
        200, 250, // Width, Height
        NULL, NULL, hInstance, NULL
    );
    // Create controls within the dialog (animation slider only)
    // Animation speed (vertical)
    hSliderAnimDlg = CreateWindowExA(
        0, TRACKBAR_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | TBS_VERT,
        40, 20, 20, 150,
        hControlDialog, (HMENU)ID_SLIDER_ANIM, hInstance, NULL
    );
    hSliderAnim = hSliderAnimDlg; // alias used elsewhere
    SendMessage(hSliderAnim, TBM_SETRANGE, TRUE, MAKELPARAM(1, 20));
    SendMessage(hSliderAnim, TBM_SETPOS, TRUE, ANIMATION_TICK);

    // Speed slider (horizontal)
    hSliderSpeed = CreateWindowExA(
        0, TRACKBAR_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
        10, 180, 160, 30,
        hControlDialog, (HMENU)ID_SLIDER_SPEED, hInstance, NULL
    );
    SendMessage(hSliderSpeed, TBM_SETRANGE, TRUE, MAKELPARAM(1, 20));
    SendMessage(hSliderSpeed, TBM_SETPOS, TRUE, SPEED);

    // Add Edit control for visual feedback
    char buffer[10];
    itoa(ANIMATION_TICK, buffer, 10);
    hEditAnimDlg = CreateWindowExA(0, "EDIT", buffer, 
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER, 
        80, 100, 40, 20, hControlDialog, (HMENU)ID_EDIT_ANIM, hInstance, NULL);

    ShowWindow(hControlDialog, SW_SHOW);
    UpdateWindow(hControlDialog);
}
// ---------------------------
// Entry point
// ---------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    for (int i = 0; i < NUM_SPRITES; i++) {
    sprites[i].velocityX = SPEED;
    sprites[i].velocityY = SPEED;
}
//TODO: Doesn't work last time I tried it. Come back to this later and fix it. It should be registering the common control classes needed for the trackbar sliders, but for some reason the sliders don't appear. Maybe it's a manifest issue or something else. For now, the sliders are created without using the common controls and it seems to work, but ideally we should fix this to ensure proper slider behavior and appearance across all platforms.    
// Ensure common control classes (trackbar) are registered
    //INITCOMMONCONTROLSEX icc = {0};
    //icc.dwSize = sizeof(icc);
    //icc.dwICC = ICC_BAR_CLASSES;
    //InitCommonControlsEx(&icc);
    // -----------------------
    // Register window class
    // -----------------------
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "UniversalWin32Demo";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    // -----------------------
    // Create the window
    // -----------------------
    HWND hwnd = CreateWindow(
        "UniversalWin32Demo",
        "Universal Win32 Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // start runtime timer
    startTime = GetTickCount();
    fpsLastUpdate = startTime;

    // -----------------------
    // Initialize backbuffer once
    // -----------------------
    init_backbuffer(hwnd);

    // -----------------------
    // Main game loop
    // -----------------------
    MSG msg;
    while (1)
    {
        // -------------------
        // Handle all pending messages
        // -------------------
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                cleanup_backbuffer();
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // -------------------
        // Update & render at ~60 FPS
        // -------------------
        DWORD now = GetTickCount();
        if (now - lastFrameTime >= 16)
        {
            update_game();
            render_game(hdcBackBuffer);

            // Blit to the actual window
            HDC hdcWindow = GetDC(hwnd);
            BitBlt(hdcWindow, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
                hdcBackBuffer, 0, 0, SRCCOPY);
            ReleaseDC(hwnd, hdcWindow);

            // FPS counter (updated once per second)
            fpsFrameCount++;
            if (now - fpsLastUpdate >= 1000)
            {
                fps = fpsFrameCount;
                fpsFrameCount = 0;
                fpsLastUpdate = now;

                char title[128];
                sprintf(title, "Universal Win32 Demo - FPS: %d", fps);
                SetWindowText(hwnd, title);
            }

            lastFrameTime = now;
        }
    }
}

// ---------------------------
// Initialize reusable backbuffer
// ---------------------------
void init_backbuffer(HWND hwnd)
{
    HDC hdcWindow = GetDC(hwnd);
    hdcBackBuffer = CreateCompatibleDC(hdcWindow);
    hbmBackBuffer = CreateCompatibleBitmap(hdcWindow, WINDOW_WIDTH, WINDOW_HEIGHT);
    hbmOld = (HBITMAP)SelectObject(hdcBackBuffer, hbmBackBuffer);
    ReleaseDC(hwnd, hdcWindow);
}

// ---------------------------
// Cleanup backbuffer objects
// ---------------------------
void cleanup_backbuffer()
{
    if (hdcBackBuffer)
    {
        SelectObject(hdcBackBuffer, hbmOld);
        DeleteObject(hbmBackBuffer);
        DeleteDC(hdcBackBuffer);
        hdcBackBuffer = NULL;
        hbmBackBuffer = NULL;
        hbmOld = NULL;
    }
}

// ---------------------------
// Update game state
// ---------------------------
void update_game()
{
    if (isPaused)
        return;

    for (int i = 0; i < 3; ++i)
    {
        sprites[i].x += sprites[i].velocityX;
        sprites[i].y += sprites[i].velocityY;

        if (sprites[i].x < 0 || sprites[i].x > WINDOW_WIDTH - SPRITE_SIZE) sprites[i].velocityX = -sprites[i].velocityX;
        if (sprites[i].y < 0 || sprites[i].y > WINDOW_HEIGHT - SPRITE_SIZE) sprites[i].velocityY = -sprites[i].velocityY;
    }

    // Advance animation frame at reduced frequency
    animationTick++;
    if (animationTick >= ANIMATION_TICK)
    {
        currentFrame = (currentFrame + 1) % NUM_FRAMES;
        animationTick = 0;
    }
}

// ---------------------------
// Render game to backbuffer
// ---------------------------
void render_game(HDC hdc)
{
    // Clear screen
    RECT rc = { 0,0,WINDOW_WIDTH,WINDOW_HEIGHT };
    FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));

    // Draw animated sprites
    draw_sprite(hdc, sprites[0].x, sprites[0].y, currentFrame);
    draw_sprite(hdc, sprites[1].x, sprites[1].y, (currentFrame + 2) % NUM_FRAMES);
    draw_sprite(hdc, sprites[2].x, sprites[2].y, (currentFrame + 4) % NUM_FRAMES); // phase shift

    if (isPaused)
    {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 0, 0));
        DrawText(hdc, "PAUSED (P=toggle, C=continue)", -1, &(RECT){0, 0, WINDOW_WIDTH, 30}, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Draw current sprite count in the top-left (always visible)
    char countText[64];
    sprintf(countText, "Sprites: %d", NUM_SPRITES);
    SetTextColor(hdc, RGB(0, 128, 0));
    DrawText(hdc, countText, -1, &(RECT){5, 5, WINDOW_WIDTH, 25}, DT_LEFT | DT_TOP | DT_SINGLELINE);
}

// ---------------------------
// Draw sprite with diagonal split (each square independent color cycle)
// ---------------------------
void draw_sprite(HDC hdc, int x, int y, int frame)
{
    // Two colors swap every frame for per-square independent effect
    COLORREF colorA = (frame % 2 == 0) ? RGB(255, 0, 0) : RGB(0, 0, 255);
    COLORREF colorB = (frame % 2 == 0) ? RGB(0, 0, 255) : RGB(255, 0, 0);

    POINT tri1[3] = { {x, y}, {x + SPRITE_SIZE, y}, {x, y + SPRITE_SIZE} };
    POINT tri2[3] = { {x + SPRITE_SIZE, y}, {x + SPRITE_SIZE, y + SPRITE_SIZE}, {x, y + SPRITE_SIZE} };

    HBRUSH brushA = CreateSolidBrush(colorA);
    HBRUSH brushB = CreateSolidBrush(colorB);

    // Fill first triangle (top-left half)
    SelectObject(hdc, brushA);
    Polygon(hdc, tri1, 3);

    // Fill second triangle (bottom-right half)
    SelectObject(hdc, brushB);
    Polygon(hdc, tri2, 3);

    // Choose flashing color for lines based on frame (red, green, blue)
    COLORREF flashColor;
    switch (frame % 3)
    {
    case 0: flashColor = RGB(255, 0, 0); break;
    case 1: flashColor = RGB(0, 255, 0); break;
    default: flashColor = RGB(0, 0, 255); break;
    }

    // Draw diagonal edge (flashing color)
    HPEN pen = CreatePen(PS_SOLID, 2, flashColor);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x, y, NULL);
    LineTo(hdc, x + SPRITE_SIZE, y + SPRITE_SIZE);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Draw bold flashing border on each side
    HPEN borderPen = CreatePen(PS_SOLID, 6, flashColor);
    HPEN oldBorderPen = (HPEN)SelectObject(hdc, borderPen);
    MoveToEx(hdc, x, y, NULL);
    LineTo(hdc, x + SPRITE_SIZE, y);
    MoveToEx(hdc, x + SPRITE_SIZE, y, NULL);
    LineTo(hdc, x + SPRITE_SIZE, y + SPRITE_SIZE);
    MoveToEx(hdc, x + SPRITE_SIZE, y + SPRITE_SIZE, NULL);
    LineTo(hdc, x, y + SPRITE_SIZE);
    MoveToEx(hdc, x, y + SPRITE_SIZE, NULL);
    LineTo(hdc, x, y);
    SelectObject(hdc, oldBorderPen);
    DeleteObject(borderPen);

    DeleteObject(brushA);
    DeleteObject(brushB);
}

// ---------------------------
// Window procedure
// ---------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    // Create a menu
HMENU hMenu = CreateMenu();
HMENU hSubMenu = CreatePopupMenu();

// Add menu items
AppendMenu(hSubMenu, MF_STRING, IDM_SHOW_CONTROLS, "Show Controls");
AppendMenu(hSubMenu, MF_STRING, IDM_HIDE_CONTROLS, "Hide Controls");
AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, "Controls");

// Apply the menu to the main window
SetMenu(hwnd, hMenu);
    // ... existing slider creation code (can be removed if using the new dialog) ...
    
    // Call the new function
    CreateControlDialog(((LPCREATESTRUCT)lParam)->hInstance);
    break;

    

    case WM_CLOSE:
    ShowWindow(hwnd, SW_HIDE);
    DestroyWindow(hwnd);
    PostQuitMessage(0);
    break;
    return 0;
case WM_COMMAND:
{
    int wmId = LOWORD(wParam);
    
    switch (wmId)
    {
        case IDM_SHOW_CONTROLS:
            ShowWindow(hSliderSpeed, SW_SHOW);
            ShowWindow(hSliderAnim, SW_SHOW);
            // Optionally update edit controls if you add them back
            break;
            
        case IDM_HIDE_CONTROLS:
            ShowWindow(hSliderSpeed, SW_HIDE);
            ShowWindow(hSliderAnim, SW_HIDE);
            break;
    }
    break;
}
    case WM_KEYDOWN:
        switch (wParam)
        {
        // Arrow keys control sprite 1
        case VK_LEFT:  sprites[0].velocityX = -SPEED; sprites[0].velocityY = 0; break;
        case VK_RIGHT: sprites[0].velocityX = SPEED;  sprites[0].velocityY = 0; break;
        case VK_UP:    sprites[0].velocityX = 0;      sprites[0].velocityY = -SPEED; break;
        case VK_DOWN:  sprites[0].velocityX = 0;      sprites[0].velocityY = SPEED; break;

        // WASD controls sprite 2
        case 'A':
        case 'a':     sprites[1].velocityX = -SPEED; sprites[1].velocityY = 0; break;
        case 'D':
        case 'd':     sprites[1].velocityX = SPEED;  sprites[1].velocityY = 0; break;
        case 'W':
        case 'w':     sprites[1].velocityX = 0;      sprites[1].velocityY = -SPEED; break;
        case 'S':
        case 's':     sprites[1].velocityX = 0;      sprites[1].velocityY = SPEED; break;

        case 'P':
        case 'p':
            isPaused = !isPaused;
            break;
        case 'C':
        case 'c':
            isPaused = 0;
            break;
        }
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
    
}


