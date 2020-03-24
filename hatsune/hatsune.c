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

#define BITMAP_WIDTH            100
#define BITMAP_HEIGHT           100
#define AVAIL_KEYBUF            255
#define MAX_TICK                9999
#define DEFAULT_FOREGROUND      "black"
#define DEFAULT_BACKGROUND      "white"
#define MIKU_CON1               0
#define MIKU_CON2               1
#define MIKU_DANGER1            2
#define MIKU_DANGER2            3
#define MIKU_DANGER3            4
#define MIKU_L                  5
#define MIKU_R                  6
#define MIKU_RUN1               7
#define MIKU_RUN2               8
#define MIKU_WOW                9
#define NEKO_STOP_TIME          4
#define MAXDISPLAYNAME          (64 + 5)
#define NOTDEFINED              (-1)

char    *ClassName = "Oneko";
char    *ProgramName;

Display *theDisplay;
int     theScreen;
unsigned int    theDepth;
Window  theRoot;
Window  theWindow;
char    *WindowName = NULL;
Window  theTarget = None;
char    *TargetName = NULL;
Cursor  theCursor;

unsigned int    WindowWidth;
unsigned int    WindowHeight;

XColor  theForegroundColor;
XColor  theBackgroundColor;

/* Types of animals */
#define BITMAPTYPES 1

typedef struct _AnimalDefaults {
  char *name;
  int speed, idle, bitmap_width, bitmap_height;
  long time;
  int off_x, off_y;
} AnimalDefaultsData;

AnimalDefaultsData AnimalDefaultsDataTable[] =
{
  { "neko", 13, 6, 32, 32, 125000L, 0, 0 }
};

char    *Foreground = NULL;             /*   foreground */
char    *Background = NULL;             /*   background */
long    IntervalTime = 0L;              /*   time       */
double  NekoSpeed = (double)0;          /*   speed      */
int     IdleSpace = 0;                  /*   idle       */
int     NekoMoyou = NOTDEFINED;         /*   tora       */
int     NoShape = NOTDEFINED;           /*   noshape    */
int     ReverseVideo = NOTDEFINED;      /*   reverse    */
int     ToWindow = NOTDEFINED;          /*   towindow   */
int     ToFocus = NOTDEFINED;           /*   tofocus    */
int     XOffset=0,YOffset=0;            /* X and Y offsets for cat from mouse pointer. */

Bool    DontMapped = True;

int     NekoTickCount;          /* 猫動作カウンタ */
int     NekoStateCount;         /* 猫同一状態カウンタ */
int     NekoState;              /* 猫の状態 */

int     MouseX;                 /* マウスＸ座標 */
int     MouseY;                 /* マウスＹ座標 */

int     PrevMouseX = 0;         /* 直前のマウスＸ座標 */
int     PrevMouseY = 0;         /* 直前のマウスＹ座標 */
Window  PrevTarget = None;      /* 直前の目標ウィンドウのＩＤ */

int     NekoX;                  /* 猫Ｘ座標 */
int     NekoY;                  /* 猫Ｙ座標 */

int     NekoMoveDx;             /* 猫移動距離Ｘ */
int     NekoMoveDy;             /* 猫移動距離Ｙ */

int     NekoLastX;              /* 猫最終描画Ｘ座標 */
int     NekoLastY;              /* 猫最終描画Ｙ座標 */
GC      NekoLastGC;             /* 猫最終描画 GC */
/* Variables used to set how quickly the program will chose to raise itself. */
/* Look at Interval(), Handle Visiblility Notify Event */
int     RaiseWindowDelay=0;

Pixmap  Con1Xbm, Con2Xbm, Danger1Xbm, Danger2Xbm, Danger3Xbm, LXbm, RXbm, Run1Xbm, Run2Xbm, WowXbm;
Pixmap  Con1Msk, Con2Msk, Danger1Msk, Danger2Msk, Danger3Msk, LMsk, RMsk, Run1Msk, Run2Msk, WowMsk;

GC  Con1GC, Con2GC, Danger1GC, Danger2GC, Danger3GC, LGC, RGC, Run1GC, Run2GC, WowGC;

typedef struct {
    GC          *GCCreatePtr;
    Pixmap      *BitmapCreatePtr;
    char        *PixelPattern[BITMAPTYPES];
    Pixmap      *BitmapMasksPtr;
    char        *MaskPattern[BITMAPTYPES];
} BitmapGCData;
BitmapGCData    BitmapGCDataTable[] =
{
    { &Con1GC, &Con1Xbm,  miku_connect1_bits,
      &Con1Msk, miku_connect1_mask_bits },
    { &Con2GC, &Con2Xbm,  miku_connect2_bits,
      &Con2Msk, miku_connect2_mask_bits },
    { &Danger1GC, &Danger1Xbm,  miku_danger1_bits,
      &Danger1Msk, miku_danger1_mask_bits },
    { &Danger2GC, &Danger2Xbm,  miku_danger2_bits,
      &Danger2Msk, miku_danger2_mask_bits },
    { &Danger3GC, &Danger3Xbm,  miku_danger3_bits,
      &Danger3Msk, miku_danger3_mask_bits },
    { &LGC, &LXbm,  mikul_bits,
      &LMsk, mikul_mask_bits },
    { &RGC, &RXbm,  mikur_bits,
      &RMsk, mikur_mask_bits },
    { &Run1GC, &Run1Xbm,  miku_danger3_bits,
      &Run1Msk, miku_danger3_mask_bits },
    { &Run2GC, &Run2Xbm,  miku_run1_bits,
      &Run2Msk, miku_run1_mask_bits },
    { &WowGC, &WowXbm,  miku_wow_bits,
      &WowMsk, miku_wow_mask_bits },
    { NULL, NULL, NULL, NULL, NULL }
};
typedef struct {
    GC          *TickGCPtr;
    Pixmap      *TickMaskPtr;
} Animation;

Animation       AnimationPattern[][2] =
{
  { { &Con1GC, &Con1Msk } },
  { { &Con2GC, &Con2Msk } },
  { { &Danger1GC, &Danger1Msk } },
  { { &Danger2GC, &Danger2Msk } },
  { { &Danger3GC, &Danger3Msk } },

  { { &LGC, &LMsk } },
  { { &RGC, &RMsk } },
  { { &Run1GC, &Run1Msk } },
  { { &Run2GC, &Run2Msk } },
  { { &WowGC, &WowMsk } },
};

void DrawNeko(int x, int y, Animation DrawAnime)
{
    register GC         DrawGC = *(DrawAnime.TickGCPtr);
    register Pixmap     DrawMask = *(DrawAnime.TickMaskPtr);

    if ((x != NekoLastX) || (y != NekoLastY) || (DrawGC != NekoLastGC)) 
    {
      XWindowChanges    theChanges;

      theChanges.x = x;
      theChanges.y = y;
      XConfigureWindow(theDisplay, theWindow, CWX | CWY, &theChanges);
      
      if (NoShape == False) 
      {
        XShapeCombineMask(theDisplay, theWindow, ShapeBounding, 0, 0, DrawMask, ShapeSet);
      }
      if (DontMapped) 
      {
        XMapWindow(theDisplay, theWindow);
        DontMapped = 0;
      }
      XFillRectangle(theDisplay, theWindow, DrawGC, 0, 0, BITMAP_WIDTH, BITMAP_HEIGHT);
    }

    XFlush(theDisplay);

    NekoLastX = x;
    NekoLastY = y;

    NekoLastGC = DrawGC;
}

void InitScreen(char *DisplayName)
{
  XSetWindowAttributes  theWindowAttributes;
  unsigned long         theWindowMask;
  Window                        theTempRoot;
  int                           WindowPointX;
  int                           WindowPointY;
  unsigned int          BorderWidth;
  int                           event_base, error_base;

  if ((theDisplay = XOpenDisplay(DisplayName)) == NULL) {
    fprintf(stderr, "%s: Can't open display", ProgramName);
    if (DisplayName != NULL) {
      fprintf(stderr, " %s.\n", DisplayName);
    } else {
      fprintf(stderr, ".\n");
    }
    exit(1);
  }

 // GetResources();
  char  *resource;
  int           num;
  int loop;

  Foreground = DEFAULT_FOREGROUND;
  Background = DEFAULT_BACKGROUND;
  NekoMoyou = 0;
  IntervalTime = AnimalDefaultsDataTable[NekoMoyou].time;
  NekoSpeed = (double)(AnimalDefaultsDataTable[NekoMoyou].speed);
  IdleSpace = AnimalDefaultsDataTable[NekoMoyou].idle;

  XOffset = XOffset + AnimalDefaultsDataTable[NekoMoyou].off_x;
  YOffset = YOffset + AnimalDefaultsDataTable[NekoMoyou].off_y;

  NoShape = False;
  ReverseVideo = False;
  ToWindow = False;
  ToFocus = False;
 // GetResources();

  if (!NoShape && XShapeQueryExtension(theDisplay, &event_base, &error_base) == False) {
    fprintf(stderr, "Display not suported shape extension.\n");
    NoShape = True;
  }

  theScreen = DefaultScreen(theDisplay);
  theDepth = DefaultDepth(theDisplay, theScreen);

  theRoot = RootWindow(theDisplay, theScreen);

  XGetGeometry(theDisplay, theRoot, &theTempRoot,
               &WindowPointX, &WindowPointY,
               &WindowWidth, &WindowHeight,
               &BorderWidth, &theDepth);

  // SetupColors();
  XColor      theExactColor;
  Colormap    theColormap;

  theColormap = DefaultColormap(theDisplay, theScreen);

  if (!XAllocNamedColor(theDisplay, theColormap, Foreground, &theForegroundColor, &theExactColor)) {
      fprintf(stderr, "%s: Can't XAllocNamedColor(\"%s\").\n", ProgramName, Foreground);
      exit(1);
  }

  if (!XAllocNamedColor(theDisplay, theColormap, Background, &theBackgroundColor, &theExactColor)) {
      fprintf(stderr, "%s: Can't XAllocNamedColor(\"%s\").\n", ProgramName, Background);
      exit(1);
  }
  // SetupColors();

  theWindowAttributes.background_pixel = theBackgroundColor.pixel;
  theWindowAttributes.cursor = theCursor;
  theWindowAttributes.override_redirect = True;

  if (!ToWindow) {
    XChangeWindowAttributes(theDisplay, theRoot, CWCursor, &theWindowAttributes);
  }

  theWindowMask = CWBackPixel | CWCursor | CWOverrideRedirect;

  theWindow = XCreateWindow(theDisplay, theRoot, 0, 0,
                            BITMAP_WIDTH, BITMAP_HEIGHT,
                            0, theDepth, InputOutput, CopyFromParent,
                            theWindowMask, &theWindowAttributes);

  if (WindowName == NULL) WindowName = ProgramName;
  XStoreName(theDisplay, theWindow, WindowName);

  //InitBitmapAndGCs();
  BitmapGCData        *BitmapGCDataTablePtr;
  XGCValues           theGCValues;

  theGCValues.function = GXcopy;
  theGCValues.foreground = theForegroundColor.pixel;
  theGCValues.background = theBackgroundColor.pixel;
  theGCValues.fill_style = FillTiled;
  theGCValues.ts_x_origin = 0;
  theGCValues.ts_y_origin = 0;

  for (BitmapGCDataTablePtr=BitmapGCDataTable; BitmapGCDataTablePtr->GCCreatePtr!=NULL; BitmapGCDataTablePtr++) 
  {
       *(BitmapGCDataTablePtr->BitmapCreatePtr) = XCreatePixmapFromBitmapData(theDisplay, theRoot, BitmapGCDataTablePtr->PixelPattern[NekoMoyou], BITMAP_WIDTH, BITMAP_HEIGHT, theForegroundColor.pixel, theBackgroundColor.pixel, DefaultDepth(theDisplay, theScreen));

       theGCValues.tile = *(BitmapGCDataTablePtr->BitmapCreatePtr);

       *(BitmapGCDataTablePtr->BitmapMasksPtr) = XCreateBitmapFromData(theDisplay, theRoot, BitmapGCDataTablePtr->MaskPattern[NekoMoyou], BITMAP_WIDTH, BITMAP_HEIGHT);

       *(BitmapGCDataTablePtr->GCCreatePtr) = XCreateGC(theDisplay, theWindow, GCFunction | GCForeground | GCBackground | GCTile | GCTileStipXOrigin | GCTileStipYOrigin | GCFillStyle, &theGCValues);
   }
  //InitBitmapAndGCs();

  XSelectInput(theDisplay, theWindow, ExposureMask|VisibilityChangeMask|KeyPressMask);

  XFlush(theDisplay);
}

Bool ProcessEvent()
{
    XEvent      theEvent;
    Bool        ContinueState = True;

    while (XPending(theDisplay)) 
    {
        XNextEvent(theDisplay,&theEvent);

        switch (theEvent.type) {
        case Expose:        
            if (theEvent.xexpose.count == 0) { //exe disini
                //RedrawNeko();
                XFillRectangle(theDisplay, theWindow, NekoLastGC, 0, 0, BITMAP_WIDTH, BITMAP_HEIGHT);
                XFlush(theDisplay);
                //RedrawNeko();
            }
            break;
        default:
            /* Unknown Event */
            break;
        }
    }

    return(ContinueState);
}


void ProcessNeko()
{
  NekoX = (WindowWidth - BITMAP_WIDTH / 2) / 2;
  NekoY = (WindowHeight - BITMAP_HEIGHT / 2) / 2;

  NekoTickCount = 0;
  NekoStateCount = 0;
  NekoState = MIKU_DANGER1;

  do {
    DrawNeko(NekoX, NekoY, AnimationPattern[NekoState][NekoTickCount & 0x1]);

    //TickCount();
    if (++NekoTickCount >= MAX_TICK) {
        NekoTickCount = 0;
    }

    if (NekoTickCount % 2 == 0) {
        if (NekoStateCount < MAX_TICK) {
            NekoStateCount++;
        }
    }
    //TickCount();

    //Interval();
    pause();
    if (RaiseWindowDelay>0) RaiseWindowDelay--;
    //Interval();

  } while (ProcessEvent());
}

int NekoErrorHandler(Display *dpy, XErrorEvent *err)
{
  if (err->error_code==BadWindow && (ToWindow || ToFocus)) {
    return 0;
  } else {
    char msg[80];
    XGetErrorText(dpy, err->error_code, msg, 80);
    fprintf(stderr, "%s: Error and exit.\n%s\n", ProgramName, msg);
    exit(1);
  }
}
static void NullFunction();
static void NullFunction()
{
  /* No Operation */
#if defined(SYSV) || defined(SVR4)
  signal(SIGALRM, NullFunction);
#endif /* SYSV || SVR4 */
}


int main()
{
  ProgramName = "anime";

  XSetErrorHandler(NekoErrorHandler);

  char theDisplayName[MAXDISPLAYNAME];
  InitScreen(theDisplayName);

  signal(SIGALRM, NullFunction);

  ProcessNeko();
}
