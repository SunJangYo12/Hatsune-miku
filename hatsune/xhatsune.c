#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <sys/time.h>
#include "bitmaps/miku.include"
#include "bitmasks/miku.mask.include"

#define BITMAPTYPES             1
#define BITMAP_WIDTH            100
#define BITMAP_HEIGHT           100
#define MAXDISPLAYNAME          (64 + 5)
#define MAX_TICK                9999
#define DEFAULT_FOREGROUND      "back"
#define DEFAULT_BACKGROUND      "white"
#define NOTDEFINED              (-1)
#define MIKU_STOP               0

char *ProgramName;
char *WindowName = NULL;
Display *theDisplay;
XColor theForegroundColor;
XColor theBackgroundColor;
Cursor theCursor;
Bool   DontMapped = True;

char *Foreground = NULL;
char *Background = NULL;
unsigned int WindowWidth;
unsigned int WindowHeight;
unsigned int theDepth;
long IntervalTime = 0L;
double MikuSpeed  = (double)0;
Window theRoot;
Window theWindow;
int  theScreen;
int  MikuTickCount, MikuState, MikuStateCount;
int  MikuX, MikuY;
int  MikuMoyou = NOTDEFINED;
int  NoShape = NOTDEFINED;
int  ReverseVideo = NOTDEFINED;
int  ToWindow = NOTDEFINED;
int  ToFocus = NOTDEFINED;
int  IdleSpace  = 0;
int  RaiseWindowDelay = 0;
int  XOffset=0, YOffset=0;
int  MikuLastX, MikuLastY;

GC  MikuLastGC;
GC  miku1GC;
Pixmap miku1Xbm;
Pixmap miku1Msk;

typedef struct
{
    GC      *GCCreatePtr;
    Pixmap  *BitmapCreatePtr;
    char    *PixelPattern[BITMAPTYPES];
    Pixmap  *BitmapMasksPtr;
    char    *MaskPattern[BITMAPTYPES];
} BitmapGCData;

BitmapGCData BitmapGCDataTable[] = 
{
    { &miku1GC, &miku1Xbm, miku1_bits,
      &miku1Msk, miku1_mask_bits },

    { NULL, NULL, NULL, NULL, NULL }
};

typedef struct _AnimeDefaults
{
    char *name;
    int  speed, idle, bitmpa_width, bitmpa_height;
    long time;
    int off_x, off_y;
}AnimeData;

AnimeData AnimeTable[] =
{
    { "miku", 13, 6, 300, 300, 125000L, 0, 0 }
};

typedef struct
{
    GC     *TickGCPtr;
    Pixmap *TickMaskPtr;
}Animation;

Animation AnimationPatern[][2] = 
{
    { { &miku1GC, &miku1Msk } },
};

int MikuErrorHandler(Display *dpy, XErrorEvent *err)
{
    if (err->error_code == BadWindow && (ToWindow || ToFocus)) {
        return 0;
    } 
    else {
        char msg[80];
        XGetErrorText(dpy, err->error_code, msg, 80);
        fprintf(stderr, "%s: Error and exit.\n%s\n", ProgramName, msg);
        exit(1);
    }

    Foreground = DEFAULT_FOREGROUND;
    Background = DEFAULT_BACKGROUND;
    MikuMoyou  = 0;
    IntervalTime = AnimeTable[MikuMoyou].time;
    MikuSpeed  = (double)(AnimeTable[MikuMoyou].speed);
    IdleSpace  = AnimeTable[MikuMoyou].idle;
}
static void NullFunction();
static void NullFunction()
{
#if defined(SYSV) || defined(SVR4)
    signal(SIGALRM, NullFunction);
#endif
}

void InitScreen(char *DisplayName)
{
    XSetWindowAttributes theWindowAttributes;
    unsigned long theWindowMask;
    unsigned int  BorderWidth;
    Window        theTempRoot;
    int           WindowPointX;
    int           WindowPointY;
    int           event_base, error_base;

    if ((theDisplay=XOpenDisplay(DisplayName)) == NULL)
    {
        fprintf(stderr, "%s:Can't open display\n", ProgramName);
        if (DisplayName != NULL)
        {
            fprintf(stderr, "%s.\n", DisplayName);
        } else {
            fprintf(stderr, ".\n");
        }
        exit(1);
    }

    //GetResources()
    char *resource;
    int  num;
    int  loop;

    XOffset = XOffset + AnimeTable[MikuMoyou].off_x;
    YOffset = YOffset + AnimeTable[MikuMoyou].off_y;

    NoShape = False;
    ReverseVideo = False;
    ToWindow = False;
    ToFocus = False;
    //GetResource()

    if  (!NoShape && XShapeQueryExtension(theDisplay, &event_base, &error_base) == False) {
        fprintf(stderr, "Display not supported shape extension.\n");
        NoShape = True;
    }
    theScreen = DefaultScreen(theDisplay);
    theDepth  = DefaultDepth(theDisplay, theScreen);
    theRoot   = RootWindow(theDisplay, theScreen);
    XGetGeometry(theDisplay, theRoot, &theTempRoot,
                 &WindowPointX, &WindowPointY,
                 &WindowWidth, &WindowHeight,
                 &BorderWidth, &theDepth);

    //SetupColors()
    XColor   theExactColor;
    Colormap theColormap;
    theColormap = DefaultColormap(theDisplay, theScreen);
    if (!XAllocNamedColor(theDisplay, theColormap, Foreground, &theForegroundColor, &theExactColor)) {
        fprintf(stderr, "%s: Can't XAllocNamedColor(\"%s\").\n", ProgramName, Foreground);
        exit(1);
    }
    if (!XAllocNamedColor(theDisplay, theColormap, Background, &theBackgroundColor, &theExactColor)) {
        fprintf(stderr, "%s: Can't XAllocNamedColor(\"%s\").\n", ProgramName, Background);
        exit(1);
    }
    //SetupColor

    theWindowAttributes.background_pixel = theBackgroundColor.pixel;
    theWindowAttributes.cursor = theCursor;
    theWindowAttributes.override_redirect = True;

    if (!ToWindow) {
        XChangeWindowAttributes(theDisplay, theRoot, CWCursor, &theWindowAttributes);
    }

    theWindowMask = CWBackPixel | CWCursor | CWOverrideRedirect;
    theWindow     = XCreateWindow(theDisplay, theRoot, 0, 0,
                                   BITMAP_WIDTH, BITMAP_HEIGHT,
                                   0, theDepth, InputOutput, CopyFromParent,
                                   theWindowMask, &theWindowAttributes);

    if (WindowName == NULL) WindowName = ProgramName;
    XStoreName(theDisplay, theWindow, WindowName);


    //InitBitmapAndGCs()
    BitmapGCData *BitmapGCDataTablePtr;
    XGCValues    theGCValues;

    theGCValues.function = GXcopy;
    theGCValues.foreground = theForegroundColor.pixel;
    theGCValues.background = theBackgroundColor.pixel;
    theGCValues.fill_style = FillTiled;
    theGCValues.ts_x_origin = 0;
    theGCValues.ts_y_origin = 0;

    for (BitmapGCDataTablePtr=BitmapGCDataTable; BitmapGCDataTablePtr->GCCreatePtr!=NULL; BitmapGCDataTablePtr++) 
    {
        *(BitmapGCDataTablePtr->BitmapCreatePtr) = XCreatePixmapFromBitmapData(theDisplay, theRoot, BitmapGCDataTablePtr->PixelPattern[MikuMoyou], BITMAP_WIDTH, BITMAP_HEIGHT, theForegroundColor.pixel, theBackgroundColor.pixel, DefaultDepth(theDisplay, theScreen));

        theGCValues.tile = *(BitmapGCDataTablePtr->BitmapCreatePtr);

        *(BitmapGCDataTablePtr->BitmapMasksPtr) = XCreateBitmapFromData(theDisplay, theRoot, BitmapGCDataTablePtr->MaskPattern[MikuMoyou], BITMAP_WIDTH, BITMAP_HEIGHT);

        *(BitmapGCDataTablePtr->GCCreatePtr) = XCreateGC(theDisplay, theWindow, GCFunction | GCForeground | GCBackground | GCTile | GCTileStipXOrigin | GCTileStipYOrigin | GCFillStyle, &theGCValues);
    }

    //InitBitmapAndGCs();
    XSelectInput(theDisplay, theWindow, ExposureMask|VisibilityChangeMask|KeyPressMask);
    XFlush(theDisplay);
}

void DrawMiku(int x, int y, Animation DrawAnime)
{
    register GC   DrawGC = *(DrawAnime.TickGCPtr);
    register Pixmap DrawMask = *(DrawAnime.TickMaskPtr);

    if ((x != MikuMoyou) || (y != MikuLastY) || (DrawGC != MikuLastGC))
    {
        XWindowChanges theChanges;
        theChanges.x = x;
        theChanges.y = y;

        XConfigureWindow(theDisplay, theWindow, CWX | CWY, &theChanges);

        if (NoShape == False){
            XShapeCombineMask(theDisplay, theWindow, ShapeBounding, 0, 0, DrawMask, ShapeSet);
        }
        if (DontMapped){
            XMapWindow(theDisplay, theWindow);
            DontMapped = 0;
        }
        XFillRectangle(theDisplay, theWindow, DrawGC, 0, 0, BITMAP_WIDTH, BITMAP_HEIGHT);
    }
    XFlush(theDisplay);
    MikuLastX = x;
    MikuLastY = y;
    MikuLastGC = DrawGC;
}

Bool ProsesEvent()
{
    XEvent theEvent;
    Bool   ContinueState = True;

    while (XPending(theDisplay))
    {
        XNextEvent(theDisplay, &theEvent);

        switch (theEvent.type) {
            case Expose:
            if (theEvent.xexpose.count == 0) {
                //RedrawMiku()
                XFillRectangle(theDisplay, theWindow, MikuLastGC, 0, 0, BITMAP_WIDTH, BITMAP_HEIGHT);
                XFlush(theDisplay);
                //RedrawMiku()
            }
            break;
            default:
            break;
        }
    }
    return(ContinueState);
}

void ProsesMiku()
{
    MikuX = (WindowWidth - BITMAP_WIDTH / 2) / 2;
    MikuY = (WindowHeight - BITMAP_HEIGHT / 2) / 2;

    MikuTickCount = 0;
    MikuStateCount = 0;
    MikuState = MIKU_STOP;

    do {
        DrawMiku(MikuX, MikuY, AnimationPatern[MikuState][MikuTickCount & 0x1]);
    
        //TickCount()
        if (++MikuTickCount >= MAX_TICK){
            MikuTickCount = 0;
        }
        if (MikuTickCount % 2 == 0) {
            if (MikuStateCount < MAX_TICK) {
                MikuStateCount++;
            }
        }
        //TickCount()

        //Interval()
        pause();
        if (RaiseWindowDelay > 0) RaiseWindowDelay--;
        //Interval()
    } while(ProsesEvent());
}

int main()
{
    ProgramName = "miku";

    XSetErrorHandler(MikuErrorHandler);

    char theDisplayName[MAXDISPLAYNAME];
    InitScreen(theDisplayName);

    signal(SIGALRM, NullFunction);
    ProsesMiku();
}
