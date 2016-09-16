//
//  VisualSort.c
//

#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <time.h>

// Child windows IDs
#define     ID_SORTWND              0
#define     ID_STRPAUBTN            1
#define     ID_RSTBTN               2

// Buttons size
#define     BTNS_H                  40
#define     BTNS_W                  120

// Status IDs
#define     STATUS_READY            0
#define     STATUS_INICOUNTING      1
#define     STATUS_PAUSED           2
#define     STATUS_RESUMECOUNTING   3

// Proprietary messages
#define     WM_ADDR_SET             ( WM_USER + 0 )
#define     WM_RST_SET              ( WM_USER + 1 )
#define     WM_SORT_DONE            ( WM_USER + 2 )

// Board config and size of items set
//#define     BRD_SIZE_SQ     RAND_MAX + 1    // Board size in squares (logical units)
#define     BRD_SIZE_SQ     100    // Board size in squares (logical units)

typedef struct paramsTag
{
     HANDLE hEvent;
     HWND   hMainWnd;
     HWND   hSortWnd;
     BOOL   bContinue;
     int    iStatus;
     int*   pElemsSet;
} PARAMS, *PPARAMS;

LRESULT CALLBACK WndProcMain( HWND, UINT, WPARAM, LPARAM );
LRESULT CALLBACK WndProcSort( HWND, UINT, WPARAM, LPARAM );

void Thread( PVOID pvoid );

void fillSet( int* elemsSet );
void shuffleSet( int* elemsSet );
void setUpMappingMode( HDC hdc, int cX, int cY );
void drawItem( HDC hdc, HPEN itemPen, HBRUSH itemBrush, int pos, int val );
void deleteItem( HDC hdc, int pos, int val );
void drawSet( HDC hdc, HPEN itemPen, HBRUSH itemBrush, int* elemsSet );
void drawGrid( HDC hdc );
void swapItems( int* elemsSet, int i, int j );
void swapBars( HWND hSortWnd, HPEN itemPen, HBRUSH itemBrush,
    int* elemsSet, int i, int j );
void selectionSort( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* elemsSet );
void quicksort( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* set, int l, int h );
int partition( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* set, int l, int h );

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR szCmdLine, int iCmdShow )
{
    static TCHAR szAppName[] = TEXT( "VisualSort" );
    HWND         hwnd = 0;
    WNDCLASS     wndclass = { 0 };
    MSG          msg = { 0 };

    wndclass.style         = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc   = WndProcMain;
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

LRESULT CALLBACK WndProcMain( HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam )
{
    static PARAMS params;
    static HWND hSortWnd, hStrPauBtn, hRstBtn;
    static int cxClient, cyClient;
    WNDCLASS wndclass;

    // Set of items to be sorted
    static int itemsSet[ BRD_SIZE_SQ ] = { 0 };

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
            WS_CHILD | WS_VISIBLE,
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
            TEXT( "button" ), TEXT( "Shuffle" ),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0,
            hwnd, ( HMENU )ID_RSTBTN,
            ( ( LPCREATESTRUCT )lParam )->hInstance, NULL );

        // Seed the rand function
        srand( ( unsigned int )time( NULL ) );

        // Initialize set
        fillSet( itemsSet );
        shuffleSet( itemsSet );

        // Pass address set address to visualization wnd
        SendMessage( hSortWnd, WM_ADDR_SET, 0, ( LPARAM )itemsSet );

        // Set up worker thread's params
        params.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
        params.hMainWnd = hwnd;
        params.hSortWnd = hSortWnd;
        params.bContinue = FALSE;
        params.iStatus = STATUS_READY;
        params.pElemsSet = itemsSet;

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
            if ( params.iStatus == STATUS_PAUSED ||
                 params.iStatus == STATUS_READY )
            {
                // From PAUSED to READY
                params.iStatus = STATUS_READY;
                SetWindowText( hStrPauBtn, TEXT( "Start" ) );

                SendMessage( params.hSortWnd, WM_RST_SET, 0, 0 );
            }
        }
        return 0;

    case WM_SORT_DONE :
        // Sort done
        params.iStatus = STATUS_READY;
        params.bContinue = FALSE;
        EnableWindow( hRstBtn, TRUE );
        SetWindowText( hStrPauBtn, TEXT( "Start" ) );
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
    HDC hdc;
    PAINTSTRUCT ps;
    static int  cxClient, cyClient;
    static HPEN redPen;
    static HBRUSH redBrush;
    static int* ptrItemsSet;

    switch ( message )
    {
    case WM_CREATE :
        redPen = CreatePen( PS_SOLID, 0, RGB( 0xFF, 0x00, 0x00 ) );
        redBrush = CreateSolidBrush( RGB( 0xFF, 0x00, 0x00 ) );
        return 0;

    case WM_SIZE :
        cxClient = LOWORD( lParam );
        cyClient = HIWORD( lParam );
        return 0;

    case WM_ADDR_SET :
        // Store set address
        ptrItemsSet = ( int* )lParam;
        return 0;

    case WM_PAINT :
        // Redraw set
        hdc = BeginPaint( hwnd, &ps );
        setUpMappingMode( hdc, cxClient, cyClient );
        drawSet( hdc, redPen, redBrush, ptrItemsSet );
//        drawGrid( hdc );
        EndPaint( hwnd, &ps );
        return 0 ;

    case WM_RST_SET :
        // Initialize set
        fillSet( ptrItemsSet );
        shuffleSet( ptrItemsSet );

        // Redraw set
        InvalidateRect( hwnd, NULL, TRUE );
        return 0;

    case WM_DESTROY :
        // Clean up
        DeleteObject( redPen );
        DeleteObject( redBrush );
        return 0;
    }

    return DefWindowProc( hwnd, message, wParam, lParam );
}

void Thread( PVOID pvoid )
{
    volatile PPARAMS pparams;
    static HPEN redPen;
    static HBRUSH redBrush;

    pparams = ( PPARAMS )pvoid;

    redPen = CreatePen( PS_SOLID, 0, RGB( 0xFF, 0x00, 0x00 ) );
    redBrush = CreateSolidBrush( RGB( 0xFF, 0x00, 0x00 ) );

    while ( TRUE )
    {
        WaitForSingleObject( pparams->hEvent, INFINITE );

        // Sort set, uncomment desired method

        //selectionSort( pparams->hSortWnd, &( pparams->bContinue ),
        //    pparams->iStatus, redPen, redBrush, pparams->pElemsSet );

        quicksort( pparams->hSortWnd, &( pparams->bContinue ),
            pparams->iStatus, redPen, redBrush,
            pparams->pElemsSet, 0, BRD_SIZE_SQ - 1 );

        // Report sorting done
        if ( pparams->bContinue == TRUE )
            SendMessage( pparams->hMainWnd, WM_SORT_DONE, 0, 0 );
    }

    // Clean up
    DeleteObject( redPen );
    DeleteObject( redBrush );
}

void fillSet( int* elemsSet )
{
    int i = 0;

    for ( i = 0; i < BRD_SIZE_SQ; i++ )
    {
        elemsSet[ i ] = i + 1;
    }
}

void shuffleSet( int* elemsSet )
{
    int i = 0;      // current item
    int j = 0;      // random chosen item
    int tmp = 0;    // tmp value

    // Iterate over all items
    for ( i = 0; i < BRD_SIZE_SQ; i++ )
    {
        // Choose one item randomly
        j = rand() % BRD_SIZE_SQ;
        
        // Swap current and chosen items
        tmp = elemsSet[ i ];
        elemsSet[ i ] = elemsSet[ j ];
        elemsSet[ j ] = tmp;
    }
}

void setUpMappingMode( HDC hdc, int cX, int cY )
{
    // Set up mapping mode
    SetMapMode( hdc, MM_ANISOTROPIC );

    // Set up extents
    SetWindowExtEx( hdc, BRD_SIZE_SQ, BRD_SIZE_SQ, NULL );
    SetViewportExtEx( hdc, cX - 1, cY - 1, NULL );

    // Set up 'viewport' origin
    SetViewportOrgEx( hdc, 0, 0, NULL);
}

void drawSet( HDC hdc, HPEN itemPen, HBRUSH itemBrush, int* elemsSet )
{
    int i = 0;

    for ( i = 0; i < BRD_SIZE_SQ; i++ )
    {
        drawItem( hdc, itemPen, itemBrush, i, elemsSet[ i ] );
    }
}

void drawItem( HDC hdc, HPEN itemPen, HBRUSH itemBrush, int pos, int val )
{
    // Validate args
    pos = max( 0, min( pos, BRD_SIZE_SQ - 1 ) );
    val = max( 1, min( val, BRD_SIZE_SQ ) );

    // Select pen and brush
    SelectObject( hdc, itemPen );
    SelectObject( hdc, itemBrush );
    
    // Draw item
    Rectangle( hdc, 0, pos + 1, val, pos );
}

void deleteItem( HDC hdc, int pos, int val )
{
    // Validate args
    pos = max( 0, min( pos, BRD_SIZE_SQ - 1 ) );
    val = max( 1, min( val, BRD_SIZE_SQ ) );

    // Select pen and brush
    SelectObject( hdc, GetStockObject( WHITE_PEN ) );
    SelectObject( hdc, GetStockObject( WHITE_BRUSH ) );
    
    // Draw item
    Rectangle( hdc, 0, pos + 1, val, pos );
}

void drawGrid( HDC hdc )
{
    int i;

    // Select black pen and black brush
    SelectObject( hdc, GetStockObject( BLACK_PEN ) );
    SelectObject( hdc, GetStockObject( BLACK_BRUSH ) );

    // Vertical lines, left to right, bottom to top
    for ( i = 0; i <= BRD_SIZE_SQ; i++ )
    {
        MoveToEx( hdc, i, 0, NULL );
        LineTo( hdc, i, BRD_SIZE_SQ );
    }

    // Horizontal lines, bottom to top, left to right
    for ( i = 0; i <= BRD_SIZE_SQ; i++ )
    {
        MoveToEx( hdc, 0, i, NULL );
        LineTo( hdc, BRD_SIZE_SQ, i );
    }

    // Draw again right border from top to bottom
    // so all corners of the grid are cover
    MoveToEx( hdc, BRD_SIZE_SQ, BRD_SIZE_SQ, NULL );
    LineTo( hdc, BRD_SIZE_SQ, 0 );
}

void swapBars( HWND hSortWnd, HPEN itemPen, HBRUSH itemBrush,
    int* elemsSet, int i, int j )
{
    HDC hdc;
    RECT rcClientSortWnd;
    static int cxSortWnd, cySortWnd;

    // Get sort win client area size
    GetClientRect( hSortWnd, &rcClientSortWnd );
    cxSortWnd = rcClientSortWnd.right - rcClientSortWnd.left;
    cySortWnd = rcClientSortWnd.bottom - rcClientSortWnd.top;

    hdc = GetDC( hSortWnd );

    setUpMappingMode( hdc, cxSortWnd, cySortWnd );

    deleteItem( hdc, i, elemsSet[ i ] );

    deleteItem( hdc, j, elemsSet[ j ] );

    drawItem( hdc, itemPen, itemBrush, i, elemsSet[ j ] );

    drawItem( hdc, itemPen, itemBrush, j, elemsSet[ i ] );

    ReleaseDC( hSortWnd, hdc );
}

void swapItems( int* elemsSet, int i, int j )
{
    int tmp = 0;

    tmp = elemsSet[ i ];
    elemsSet[ i ] = elemsSet[ j ];
    elemsSet[ j ] = tmp;
}

void selectionSort( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* elemsSet )
{
    int i, j;   // Set indices
    int sml;    // Smallest item found in current pass
    static int lastI;

    if ( iStatus == STATUS_INICOUNTING )
        lastI = 0;

    // Loop over array size - 1 items
    for ( i = lastI; i < BRD_SIZE_SQ - 1 && (*pbContinue); i++ )
    {
        sml = i;    // Initialize smallest elem found

        // Loop over remaining array
        for ( j = i + 1; j < BRD_SIZE_SQ && (*pbContinue); j++ )
        {
            if ( elemsSet[ j ] < elemsSet[ sml ] )
            {
                sml = j;
            }
        }
        
        // Swap smallest and current analysed item on graphic
        swapBars( hSortWnd, itemPen, itemBrush, elemsSet, i, sml );

        // Swap smallest and current analysed item on array
        swapItems( elemsSet, i, sml );

        Sleep( 150 );
    }

    if ( (*pbContinue) == FALSE )
        lastI = i;
}

void quicksort( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* set, int l, int h )
{
    int p = 0;

    if ( l < h )
    {
        p = partition( hSortWnd, pbContinue, iStatus, itemPen, itemBrush,
                set, l, h );

        quicksort( hSortWnd, pbContinue, iStatus, itemPen, itemBrush,
            set, l, p - 1 );
        
        quicksort( hSortWnd, pbContinue, iStatus, itemPen, itemBrush,
            set, p + 1, h );
    }
}

int partition( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* set, int l, int h )
{
    int pivot = set[ h ];
    int i = l;
    int j = l;

    for ( j = l; j < h; j++ )
    {
        if ( set[ j ] <= pivot )
        {
            swapBars( hSortWnd, itemPen, itemBrush, set, i, j );
            swapItems( set, i, j );
            i++;

            Sleep( 50 );
        }
    }

    swapBars( hSortWnd, itemPen, itemBrush, set, i, h );
    swapItems( set, i, h );

    Sleep( 50 );

    return i;
}