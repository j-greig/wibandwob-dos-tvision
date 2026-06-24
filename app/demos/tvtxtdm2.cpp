// TVTXTDM2.CPP - Text file viewer demo (Borland, 1992, Michael Shunfenthal)
// Ported to modern magicius/tvision
//
// Original: displays a text file in a TTerminal window.
// Heavily rewritten: removed DOS-specific I/O (io.h, alloc.h, coreleft,
// farcoreleft, open/filelength/close, #pragma exit, ostrstream/ifstream
// old Borland headers) and replaced with modern C++ I/O.

#define Uses_TWindow
#define Uses_TApplication
#define Uses_TRect
#define Uses_TTerminal
#define Uses_MsgBox
#define Uses_otstream
#define Uses_TDeskTop
#define Uses_TProgram
#include <tvision/tv.h>

#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>

class TTerminalWindow : public TWindow
{
public:
    TTerminalWindow( TRect bounds,
                     const char *winTitle,
                     ushort windowNo,
                     TTerminal *&interior,
                     ushort aBufSize
                   );
    TTerminal *makeInterior( TRect bounds, ushort aBufSize );
};

class TMyApp : public TApplication
{
public:
    TMyApp( int argc, char *argv[] );
    void showTerminalWindow( const char *fileName );
};


TTerminalWindow::TTerminalWindow( TRect bounds,
                                  const char *winTitle,
                                  ushort windowNo,
                                  TTerminal *&interior,
                                  ushort aBufSize
                                ) :
    TWindowInit( &TTerminalWindow::initFrame ),
    TWindow(bounds, winTitle, windowNo )
{
    interior = makeInterior( bounds, aBufSize );
    insert( interior );
}

TTerminal *TTerminalWindow::makeInterior( TRect bounds, ushort aBufSize )
{
    bounds = getExtent();
    bounds.grow( -1, -1 );
    return new TTerminal( bounds,
        0,
        standardScrollBar( sbVertical | sbHandleKeyboard ),
        aBufSize );
}

TMyApp::TMyApp( int argc, char *argv[] ) :
    TProgInit( &TMyApp::initStatusLine,
               &TMyApp::initMenuBar,
               &TMyApp::initDeskTop
             )
{
    if( argc < 2 )
    {
        std::cerr << "Syntax: tvtxtdm2 <file to view>" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Check if file can be opened
    std::ifstream testFile(argv[1]);
    if( !testFile.is_open() )
    {
        std::cerr << "Cannot open file: " << argv[1] << std::endl;
        exit(EXIT_FAILURE);
    }
    testFile.close();

    showTerminalWindow( argv[1] );
}

void TMyApp::showTerminalWindow( const char *fileName )
{
    unsigned buffSize = 8192;

    // Get file size
    std::ifstream sizeCheck(fileName, std::ios::binary | std::ios::ate);
    std::streamsize fileSize = sizeCheck.tellg();
    sizeCheck.close();

    if( fileSize > buffSize )
    {
        messageBox(
            mfOKButton | mfWarning,
            "File is too big to fit in a TTerminal buffer. "
            "Only the first %u bytes of the file will be displayed.",
            buffSize );
    }
    else
    {
        buffSize = (unsigned)fileSize;
    }

    // Initialize the terminal window object
    TTerminal *interior;
    TTerminalWindow *demo = new TTerminalWindow( TRect( 10, 1, 70, 18 ),
                                                 fileName,
                                                 wnNoNumber,
                                                 interior,
                                                 buffSize
                                               );
    deskTop->insert( demo );

    // Read and display the file
    otstream os( interior );

    std::ifstream is( fileName );
    char st[128];

    while( is.getline( st, sizeof st ) && interior->canInsert(strlen(st)) )
        os << st << "\n";

    interior->scrollTo(0, 0);
}

int main( int argc, char *argv[] )
{
    TMyApp app( argc, argv );
    app.run();
    return 0;
}
