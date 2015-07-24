#include "cps.h"
// CPS - Draw

UINT8 CpsRecalcPal = 0;			// Flag - If it is 1, recalc the whole palette

static INT32 LayerCont;
INT32 nStartline, nEndline;
INT32 nRasterline[MAX_RASTER + 2];

INT32 nCpsLcReg = 0;						// Address of layer controller register
INT32 CpsLayEn[6] = {0, 0, 0, 0, 0, 0};	// bits for layer enable
INT32 MaskAddr[4] = {0, 0, 0, 0};

INT32 CpsLayer1XOffs = 0;
INT32 CpsLayer2XOffs = 0;
INT32 CpsLayer3XOffs = 0;
INT32 CpsLayer1YOffs = 0;
INT32 CpsLayer2YOffs = 0;
INT32 CpsLayer3YOffs = 0;

INT32 Cps1DisableBgHi = 0;
INT32 CpsDisableRowScroll = 0;

INT32 Cps1OverrideLayers = 0;
INT32 nCps1Layers[4] = { -1, -1, -1, -1 };
INT32 nCps1LayerOffs[3] = { -1, -1, -1 };

static void Cps1Layers();

typedef INT32  (*CpsObjDrawDoFn)(INT32,INT32);
typedef INT32  (*CpsScrXDrawDoFn)(UINT8 *,INT32,INT32);
typedef void (*CpsLayersDoFn)();
typedef INT32  (*CpsrPrepareDoFn)();
typedef INT32  (*CpsrRenderDoFn)();

CpsObjDrawDoFn  CpsObjDrawDoX;
CpsScrXDrawDoFn CpsScr1DrawDoX;
CpsScrXDrawDoFn CpsScr3DrawDoX;
CpsLayersDoFn   CpsLayersDoX;
CpsrPrepareDoFn CpsrPrepareDoX;
CpsrRenderDoFn  CpsrRenderDoX;

void DrawFnInit()
{
   CpsLayersDoX   = Cps1Layers;
   CpsScr1DrawDoX = Cps1Scr1Draw;
   CpsScr3DrawDoX = Cps1Scr3Draw;
   CpsObjDrawDoX  = Cps1ObjDraw;
   CpsrPrepareDoX = Cps1rPrepare;
   CpsrRenderDoX  = Cps1rRender;
}

static INT32 DrawScroll1(INT32 i)
{
	// Draw Scroll 1
	INT32 nOff, nScrX, nScrY;
	UINT8 *Find;

	nOff = BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x02)));
	if (Cps1OverrideLayers && nCps1LayerOffs[0] != -1) {
		nOff = BURN_ENDIAN_SWAP_INT16(nCps1LayerOffs[0]);
	}

	// Get scroll coordinates
	nScrX = BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x0c))); // Scroll 1 X
	nScrY = BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x0e))); // Scroll 1 Y

	nScrX += 0x40;

//	bprintf(PRINT_NORMAL, _T("1 %x, %x, %x\n"), nOff, nScrX, nScrY);

	nScrX += CpsLayer1XOffs;
	nScrY += 0x10;
	nScrY += CpsLayer1YOffs;
	nOff <<= 8;
	nOff &= 0xffc000;
	Find = CpsFindGfxRam(nOff, 0x4000);
	if (!Find)
		return 1;
	CpsScr1DrawDoX(Find, nScrX, nScrY);
	return 0;
}

static INT32 DrawScroll2Init(INT32 i)
{
	// Draw Scroll 2
	INT32 nScr2Off; INT32 n;

	nScr2Off = BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x04)));
	if (Cps1OverrideLayers && nCps1LayerOffs[1] != -1)
		nScr2Off = BURN_ENDIAN_SWAP_INT16(nCps1LayerOffs[1]);

	// Get scroll coordinates
	nCpsrScrX= BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x10))); // Scroll 2 X
	nCpsrScrY= BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x12))); // Scroll 2 Ytess

	// Get row scroll information
	n = BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x22)));

	nScr2Off <<= 8;

	nCpsrScrX += 0x40;

//	bprintf(PRINT_NORMAL, _T("2 %x, %x, %x\n"), nScr2Off, nCpsrScrX, nCpsrScrY);

	nCpsrScrX += CpsLayer2XOffs;
	nCpsrScrX &= 0x03FF;

	nCpsrScrY += 0x10;
	nCpsrScrY += CpsLayer2YOffs;
	nCpsrScrY &= 0x03FF;

	nScr2Off &= 0xFFC000;
	CpsrBase = CpsFindGfxRam(nScr2Off, 0x4000);
	if (!CpsrBase)
		return 1;

	CpsrRows = NULL;

	if ((n & 1) && !CpsDisableRowScroll)
   {
      // Find row scroll table:
      INT32 nTab = BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x08)));
      INT32 nStart = BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x20)));

      nTab <<= 8;
      nTab &= 0xFFF800; // Vampire - Row scroll effect in VS screen background

      CpsrRows = (UINT16 *)CpsFindGfxRam(nTab, 0x0800);

      // Find start offset
      nCpsrRowStart = nStart + 16;
   }

	CpsrPrepareDoX();
	return 0;
}

inline static INT32 DrawScroll2Exit()
{
	CpsrBase = NULL;
	nCpsrScrX = 0;
	nCpsrScrY = 0;
	CpsrRows = NULL;
	return 0;
}

inline static INT32 DrawScroll2Do()
{
	if (!CpsrBase)
		return 1;
	CpsrRenderDoX();
	return 0;
}

static INT32 DrawScroll3(INT32 i)
{
	// Draw Scroll 3
	INT32 nOff, nScrX, nScrY;
	UINT8 *Find;

	nOff = BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x06)));
	if (Cps1OverrideLayers && nCps1LayerOffs[2] != -1)
		nOff = BURN_ENDIAN_SWAP_INT16(nCps1LayerOffs[2]);

	// Get scroll coordinates
	nScrX = BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x14))); // Scroll 3 X
	nScrY = BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[i] + 0x16))); // Scroll 3 Y

	nScrX += 0x40;

//	bprintf(PRINT_NORMAL, _T("3 %x, %x, %x\n"), nOff, nScrX, nScrY);

	nScrX += CpsLayer3XOffs;
	nScrY += 0x10;
	nScrY += CpsLayer3YOffs;

	nOff <<= 8;
	nOff &= 0xffc000;
	Find=CpsFindGfxRam(nOff, 0x4000);
	if (!Find)
		return 1;
	CpsScr3DrawDoX(Find, nScrX, nScrY);
	return 0;
}

static INT32 DrawStar(INT32 nLayer)
{
	INT32 nStar, nStarXPos, nStarYPos, nStarColour;
	UINT8* pStar = CpsStar + (nLayer << 12);

	for (nStar = 0; nStar < 0x1000; nStar++) {
		nStarColour = pStar[nStar];

		if (nStarColour != 0x0F) {
			nStarXPos = (((nStar >> 8) << 5) - *((INT16*)(CpsSaveReg[0] + 0x18 + (nLayer << 2))) + (nStarColour & 0x1F) - 64) & 0x01FF;
			nStarYPos = ((nStar & 0xFF) - *((INT16*)(CpsSaveReg[0] + 0x1A + (nLayer << 2))) - 16) & 0xFF;

			if (nStarXPos < 384 && nStarYPos < 224) {
				nStarColour = ((nStarColour & 0xE0) >> 1) + ((GetCurrentFrame() >> 4) & 0x0F);
				PutPix(pBurnDraw + (nBurnPitch * nStarYPos) + (nBurnBpp * nStarXPos), CpsPal[0x0800 + (nLayer << 9) + nStarColour]);
			}
		}
	}

	return 0;
}

static void Cps1Layers()
{
  INT32 Draw[4]={-1,-1,-1,-1};
  INT32 nDrawMask=0;
  INT32 i=0;

  nDrawMask=1; // Sprites always on
  LayerCont = BURN_ENDIAN_SWAP_INT16(*((UINT16 *)(CpsSaveReg[0] + nCpsLcReg)));
  // Get correct bits from Layer Controller
  if (LayerCont & CpsLayEn[1])
     nDrawMask|=2;
  if (LayerCont & CpsLayEn[2])
     nDrawMask|=4;
  if (LayerCont & CpsLayEn[3])
     nDrawMask|=8;
  nDrawMask&=nBurnLayer;   // User choice of layers to display
  
  // Layer control:
  Draw[0]=(LayerCont>>12)&3; // top layer
  Draw[1]=(LayerCont>>10)&3;
  Draw[2]=(LayerCont>> 8)&3;
  Draw[3]=(LayerCont>> 6)&3; // bottom layer (most covered up)
  
  if (Cps1OverrideLayers) {
	nDrawMask = 1;
	Draw[0] = nCps1Layers[0];
	Draw[1] = nCps1Layers[1];
	Draw[2] = nCps1Layers[2];
	Draw[3] = nCps1Layers[3];
	if (Draw[1] != -1) nDrawMask |= 2;
	if (Draw[2] != -1) nDrawMask |= 4;
	if (Draw[3] != -1) nDrawMask |= 8;
	nDrawMask &= nBurnLayer;
  }
  
  // Check for repeated layers and if there are any, the lower layer is omitted
#define CRP(a,b) if (Draw[a]==Draw[b]) Draw[b]=-1;
  CRP(0,1) CRP(0,2) CRP(0,3) CRP(1,2) CRP(1,3) CRP(2,3)
#undef CRP

  for (i = 0; i < 2; i++)
  {
	  if (LayerCont & CpsLayEn[4 + i])
		  DrawStar(i);
  }

  // prepare layer 2
  DrawScroll2Init(0);

  // draw layers, bottom -> top
  for (i=3;i>=0;i--)
  {
    INT32 n=Draw[i]; // Find out which layer to draw

    if (n==0)
    {
       if (nDrawMask & 1)
          CpsObjDrawDoX(0,7);

       if (!Cps1DisableBgHi)
       {
          nBgHi=1;
          switch (Draw[i+1])
          {
             case 1:
                if (nDrawMask & 2) 	DrawScroll1(0);
                break;
             case 2:
                if (nDrawMask & 4)  DrawScroll2Do();
                break;
             case 3:
                if (nDrawMask & 8)  DrawScroll3(0);
                break;
          }
          nBgHi=0;
       }
    }

    // Then Draw the scroll layer on top
    switch (n)
    {
       case 1:
          if (nDrawMask & 2)
             DrawScroll1(0);
          break;
       case 2:
          if (nDrawMask & 4)
             DrawScroll2Do();
          break;
       case 3:
          if (nDrawMask & 8)
             DrawScroll3(0);
          break;
    }
  }

  DrawScroll2Exit();
}

void CpsClearScreen()
{
   UINT32* pClear = (UINT32*)pBurnDraw;
   UINT32 nColour = CpsPal[0xbff ^ 15] | CpsPal[0xbff ^ 15] << 16;
   for (INT32 i = 0; i < 384 * 224 / 16; i++) {
      *pClear++ = nColour;
      *pClear++ = nColour;
      *pClear++ = nColour;
      *pClear++ = nColour;
      *pClear++ = nColour;
      *pClear++ = nColour;
      *pClear++ = nColour;
      *pClear++ = nColour;
   }
}

static void DoDraw(INT32 Recalc)
{
	CtvReady();								// Point to correct tile drawing functions

	if (bCpsUpdatePalEveryFrame) GetPalette(0, 6);
	if (Recalc || bCpsUpdatePalEveryFrame) CpsPalUpdate(CpsSavePal);		// recalc whole palette if needed
	
	CpsClearScreen();

	CpsLayersDoX();
}

INT32 CpsDraw()
{
	DoDraw(CpsRecalcPal);

	CpsRecalcPal = 0;
	return 0;
}

INT32 CpsRedraw()
{
	DoDraw(1);

	CpsRecalcPal = 0;
	return 0;
}

