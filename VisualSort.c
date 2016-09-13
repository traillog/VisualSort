//
//  VisualSort.c
//

#include <windows.h>
#include <stdio.h>
#include <process.h>

#define         ID_SORTWND              0
#define         ID_STRPAUBTN            1
#define         ID_RSTBTN               2

#define         BTNS_H                  40
#define         BTNS_W                  120

#define         STATUS_READY            0
#define         STATUS_INICOUNTING      1
#define         STATUS_PAUSED           2
#define         STATUS_RESUMECOUNTING   3

#define         WM_RST_SORT             ( WM_USER + 0 )

typedef struct paramsTag
{
     HANDLE hEvent;
     HWND   hSortWnd;
     BOOL   bContinue;
     INT    iStatus;
} PARAMS, *PPARAMS;

LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT CALLBACK WndProcSort( HWND, UINT, WPARAM, LPARAM );

void Thread( PVOID pvoid );

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR szCmdLine, int iCmdShow )
{
    static TCHAR szAppName[] = TEXT( "VisualSort" );
    HWND         hwnd = 0;
    WNDCLASS     wndclass = { 0 };
    MSG          msg = { 0 };

    wndclass.style         = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc   = WndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = hInstance;
    wndclass.hIcon         = LoadIcon( NULL, IDI_APPLICATION );
    wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wndclass.hbrBackground = ( HBRUSH )GetStockObject( WHITE_BRUSH );
    wndclass.lpszMenuName  = NULL;
    wndclass.lpszClassName = szAppName;

    if ( !RegisterClass( &wndclass ) )
    {
        MessageBox( NULL, TEXT( "This program requires Windows NT!" ),
            szAppName, MB_ICONERROR) ;
        return 0;
    }

    hwnd = CreateWindow(
        szAppName,
        TEXT( "Visual Sort" ),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 500,
        NULL,
        NULL,
        hInstance,
        NULL );

    ShowWindow( hwnd, iCmdShow );
    UpdateWindow( hwnd );

    while ( GetMessage( &msg, NULL, 0, 0 ) )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    return msg.wParam;
}

LRESULT CALLBACK WndProc( HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam )
{
    static PARAMS params;
    static HWND hSortWnd, hStrPauBtn, hRstBtn;
    static int cxClient, cyClient;
    WNDCLASS wndclass;

    switch ( message )
    {
    case WM_CREATE :
        // Config and register class for child win for running clock
        wndclass.style         = CS_HREDRAW | CS_VREDRAW;
        wndclass.cbClsExtra    = 0;
        wndclass.cbWndExtra    = 0;
        wndclass.hInstance     = ( ( LPCREATESTRUCT )lParam )->hInstance;
        wndclass.hIcon         = NULL;
        wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
        wndclass.hbrBackground = ( HBRUSH )GetStockObject( WHITE_BRUSH );
        wndclass.lpszMenuName  = NULL;
        wndclass.lpfnWndProc   = WndProcSort;
        wndclass.lpszClassName = TEXT( "SortWnd" );

        RegisterClass( &wndclass );

        // Create child win for sorting visualization
        hSortWnd = CreateWindow(
            TEXT( "SortWnd" ), NULL,
            WS_CHILD | WS_BORDER | WS_VISIBLE,
            0, 0, 0, 0, 
            hwnd, ( HMENU )ID_SORTWND,
            ( ( LPCREATESTRUCT )lParam )->hInstance, NULL) ;

        // Create 'Start/Pause' button
        hStrPauBtn = CreateWindow(
            TEXT( "button" ), TEXT( "Start" ),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0,
            hwnd, ( HMENU )ID_STRPAUBTN,
            ( ( LPCREATESTRUCT )lParam )->hInstance, NULL );
        
        // Create 'Reset' button
        hRstBtn = CreateWindow(
            TEXT( "button" ), TEXT( "Reset" ),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0,
            hwnd, ( HMENU )ID_RSTBTN,
            ( ( LPCREATESTRUCT )lParam )->hInstance, NULL );

        // Set up worker thread's params
        params.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
        params.bContinue = FALSE;
        params.iStatus = STATUS_READY;

        // Start worker thread (paused)
        _beginthread( Thread, 0, &params );
        return 0;

    case WM_SIZE :
        // Get the size of the client area
        cxClient = LOWORD( lParam );
        cyClient = HIWORD( lParam );

        // Position 'sorting visualization' window
        MoveWindow(
            hSortWnd,
            20, 20,
            cxClient - 40, cyClient - 100,
            TRUE );

        // Position 'Start/Pause' button
        MoveWindow(
            hStrPauBtn,
            cxClient / 2 - BTNS_W - 10, cyClient - 60,
            BTNS_W, BTNS_H,
            TRUE );

        // Position 'Reset' button
        MoveWindow(
            hRstBtn,
            cxClient / 2 + 10, cyClient - 60,
            BTNS_W, BTNS_H,
            TRUE );
        return 0;

    case WM_COMMAND :
        if ( LOWORD( wParam ) == ID_STRPAUBTN &&
             HIWORD( wParam ) == BN_CLICKED )
        {
            // Start/Pause button pressed
            if ( params.iStatus == STATUS_READY )
            {
                // From READY to INICOUNTING
                params.iStatus = STATUS_INICOUNTING;
                params.bContinue = TRUE;

                SetEvent( params.hEvent );

                EnableWindow( hRstBtn, FALSE );
                SetWindowText( hStrPauBtn, TEXT( "Pause" ) );
            }
            else if ( params.iStatus == STATUS_INICOUNTING )
            {
                // From INICOUNTING to PAUSED
                params.iStatus = STATUS_PAUSED;
                params.bContinue = FALSE;

                EnableWindow( hRstBtn, TRUE );
                SetWindowText( hStrPauBtn, TEXT( "Resume" ) );
            }
            else if ( params.iStatus == STATUS_PAUSED )
            {
                // From PAUSED to RESUMECOUNTING
                params.iStatus = STATUS_RESUMECOUNTING;
                params.bContinue = TRUE;

                SetEvent( params.hEvent );

                EnableWindow( hRstBtn, FALSE );
                SetWindowText( hStrPauBtn, TEXT( "Pause" ) );
            }
            else if ( params.iStatus == STATUS_RESUMECOUNTING )
            {
                // From RESUMECOUNTING to PAUSE
                params.iStatus = STATUS_PAUSED;
                params.bContinue = FALSE;

                EnableWindow( hRstBtn, TRUE );
                SetWindowText( hStrPauBtn, TEXT( "Resume" ) );
            }
        }

        if ( LOWORD( wParam ) == ID_RSTBTN &&      
             HIWORD( wParam ) == BN_CLICKED )
        {
            // Reset button pressed
            if ( params.iStatus == STATUS_PAUSED )
            {
                // From PAUSED to READY
                params.iStatus = STATUS_READY;
                SetWindowText( hStrPauBtn, TEXT( "Start" ) );

                SendMessage( params.hSortWnd, WM_RST_SORT, 0, 0 );
            }
        }
        return 0;

    case WM_DESTROY :
        // Clean up
        params.bContinue = FALSE;

        // Close the application
        PostQuitMessage( 0 );
        return 0;
    }

    return DefWindowProc( hwnd, message, wParam, lParam );
}

LRESULT CALLBACK WndProcSort( HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam )
{
    switch ( message )
    {
    case WM_RST_SORT :

        InvalidateRect( hwnd, NULL, TRUE );
        return 0;
    }

    return DefWindowProc( hwnd, message, wParam, lParam );
}

void Thread( PVOID pvoid )
{
    volatile PPARAMS pparams;

    pparams = ( PPARAMS )pvoid;

    while ( TRUE )
    {
        WaitForSingleObject( pparams->hEvent, INFINITE );

    }
}