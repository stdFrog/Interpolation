#include <windows.h>
#define CLASS_NAME	L"Interpolation"

void* MyLoadBitmap(BITMAPINFOHEADER* ih);
void Interpolation(HWND hWnd, BITMAPINFOHEADER sih, HBITMAP& hBitmap);
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow){
	WNDCLASS wc = {
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,0,
		hInst,
		NULL, LoadCursor(NULL, IDC_ARROW),
		(HBRUSH)(COLOR_WINDOW+1),
		NULL,
		CLASS_NAME
	};

	RegisterClass(&wc);

	HWND hWnd = CreateWindow(
				CLASS_NAME,
				CLASS_NAME,
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, 460, 630,
				NULL,
				(HMENU)NULL,
				hInst,
				NULL
			);

	ShowWindow(hWnd, nCmdShow);

	MSG msg;
	while(GetMessage(&msg, NULL, 0,0)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

static BYTE *OriginData = NULL; 
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam){
	static BITMAPINFOHEADER sih;
	static HBITMAP hBitmap;

	PAINTSTRUCT ps;
	HDC hdc, hMemDC;
	HGDIOBJ hOld;
	BITMAP bmp;
	RECT crt;

	switch(iMessage){
		case WM_RBUTTONDOWN:
			OriginData = (BYTE*)MyLoadBitmap(&sih);
			if(OriginData != NULL){
				hdc = GetDC(hWnd);
				hMemDC = CreateCompatibleDC(hdc);
				if(hBitmap != NULL){
					DeleteObject(hBitmap);
					hBitmap = NULL;
				}
				hBitmap = CreateCompatibleBitmap(hdc, sih.biWidth, sih.biHeight);

				hOld = SelectObject(hMemDC, hBitmap);
				SetDIBitsToDevice(hMemDC, 0,0, sih.biWidth, sih.biHeight, 0,0,0, sih.biHeight, OriginData, (BITMAPINFO*)&sih, DIB_RGB_COLORS);

				SelectObject(hMemDC, hOld);
				DeleteDC(hMemDC);
				ReleaseDC(hWnd, hdc);
			}
			SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0);
			return 0;

		case WM_EXITSIZEMOVE:
			if(hBitmap){
				Interpolation(hWnd, sih, hBitmap);
			}
			return 0;

		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			if(hBitmap){
				GetClientRect(hWnd, &crt);

				hMemDC = CreateCompatibleDC(hdc);
				hOld = SelectObject(hMemDC, hBitmap);

				GetObject(hBitmap, sizeof(BITMAP), &bmp);
				StretchBlt(hdc, 0,0, crt.right, crt.bottom, hMemDC, 0,0, bmp.bmWidth, bmp.bmHeight, SRCCOPY);

				SelectObject(hMemDC, hOld);
				DeleteDC(hMemDC);
			}
			EndPaint(hWnd, &ps);
			return 0;

		case WM_DESTROY:
			if(OriginData)	{ free(OriginData); }
			if(hBitmap)		{ DeleteObject(hBitmap); }
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProc(hWnd, iMessage, wParam, lParam);
}

void Interpolation(HWND hWnd, BITMAPINFOHEADER sih, HBITMAP& hBitmap){
	RECT crt;

	int CurrentWidth,
		CurrentHeight,
		BitmapWidth,
		BitmapHeight,
		CurrentAlign,
		BitmapAlign,
		BitsPerPel;

	double RateWidth,
		   RateHeight;

	BITMAPINFO *pbi = NULL;
	BITMAPFILEHEADER *fh = NULL;
	BITMAPINFOHEADER ih = sih;

	pbi = (BITMAPINFO*)malloc(ih.biSize);
	pbi->bmiHeader = ih;

	HDC hdc = GetDC(hWnd);
	GetDIBits(hdc, hBitmap, 0, ih.biHeight, NULL, pbi, DIB_RGB_COLORS);
	ih = pbi->bmiHeader;

	GetClientRect(hWnd, &crt);
	CurrentWidth = crt.right - crt.left;
	CurrentHeight = crt.bottom - crt.top;

	BitmapWidth = ih.biWidth;
	BitmapHeight = ih.biHeight;
	BitsPerPel = ih.biBitCount;

	// DWORD(32비트) 정렬
	CurrentAlign = ((CurrentWidth * BitsPerPel) + 31 & ~31) >> 3;
	BitmapAlign = ((BitmapWidth * BitsPerPel) + 31 & ~31) >> 3;

	RateWidth = (double)CurrentWidth / (double)BitmapWidth;
	RateHeight = (double)CurrentHeight / (double)BitmapHeight;

	BYTE *CopyData = (BYTE*)malloc(BitmapAlign * BitmapHeight),
		 *NewData = (BYTE*)malloc(CurrentAlign * CurrentHeight);

	memcpy(CopyData, OriginData, sizeof(BYTE) * BitmapAlign * BitmapHeight);

	// 색상 채널(RGB) : 3바이트
	int Channel = 3;
	for(int i=0; i<CurrentHeight; i++){
		for(int j=0; j<CurrentWidth; j++){
			double px = j / RateWidth,
				   py = i / RateHeight;

			POINT Pixel = { (int)px, (int)py };

			double dx = px - Pixel.x,
				   dy = py - Pixel.y;

			int p1,p2,p3,p4;
			double t1,t2,t3,t4;

			t1 = (1 - dx) * (1 - dy);
			p1 = Pixel.x * Channel + Pixel.y * BitmapAlign;

			t2 = dx * (1 - dy);
			p2 = (Pixel.x + 1) * Channel + Pixel.y * BitmapAlign;

			t3 = (1 - dx) * dy;
			p3 = Pixel.x * Channel + (Pixel.y + 1) * BitmapAlign;

			t4 = dx * dy;
			p4 = (Pixel.x + 1) * Channel + (Pixel.y + 1) * BitmapAlign;

			for(int k=0; k<Channel; k++){
				double NewColorBit = (t1 * CopyData[p1 + k])
					+ (t2 * CopyData[p2 + k])
					+ (t3 * CopyData[p3 + k])
					+ (t4 * CopyData[p4 + k]);

				NewData[i * CurrentAlign + j * Channel + k] = (BYTE)NewColorBit;
			}
		}
	}

	HDC hMemDC = CreateCompatibleDC(hdc);
	if(hBitmap != NULL){
		DeleteObject(hBitmap);
		hBitmap = NULL;
	}
	hBitmap = CreateCompatibleBitmap(hdc, CurrentWidth, CurrentHeight);
	HGDIOBJ hOld = SelectObject(hMemDC, hBitmap);

	BITMAPINFOHEADER nih = ih;
	nih.biWidth = CurrentWidth;
	nih.biHeight = CurrentHeight;

	SetDIBitsToDevice(hMemDC, 0,0, nih.biWidth, nih.biHeight, 0,0,0, nih.biHeight, NewData, (BITMAPINFO*)&nih, DIB_RGB_COLORS);
	
	SelectObject(hMemDC, hOld);
	DeleteDC(hMemDC);
	ReleaseDC(hWnd, hdc);

	free(CopyData);
	free(NewData);

	InvalidateRect(hWnd, &crt, FALSE);
}

void* MyLoadBitmap(BITMAPINFOHEADER* ih){
	void *buf = NULL;
	WCHAR lpstrFile[MAX_PATH] = L"";
	WCHAR FileName[MAX_PATH];
	WCHAR InitDir[MAX_PATH];
	WCHAR *path[MAX_PATH];
	WCHAR *pt = NULL;
	OPENFILENAME ofn;

	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = lpstrFile;
	ofn.lpstrFilter = L"비트맵 파일(*.bmp)\0*.bmp\0\0";
	ofn.lpstrTitle= L"비트맵 파일을 선택하세요";
	ofn.lpstrDefExt = L"bmp";
	ofn.nMaxFile = MAX_PATH;
	ofn.nMaxFileTitle = MAX_PATH;
	ofn.hwndOwner = NULL;

	GetWindowsDirectory(InitDir, MAX_PATH);
	ofn.lpstrInitialDir = InitDir;

	if(GetOpenFileName(&ofn) != 0)
	{
		if(wcscmp(lpstrFile + ofn.nFileExtension, L"bmp") == 0)
		{
			HANDLE hFile = CreateFile(lpstrFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

			if(hFile != INVALID_HANDLE_VALUE)
			{
				DWORD dwRead;
				SetFilePointer(hFile, sizeof(BITMAPFILEHEADER), NULL, FILE_BEGIN);
				if(ReadFile(hFile, ih, sizeof(BITMAPINFOHEADER), &dwRead, NULL))
				{
					if(ih->biSizeImage == 0)
					{
						ih->biSizeImage = (((ih->biBitCount * ih->biWidth + 31) & ~31) >> 3) * ih->biHeight;
					}

					buf = malloc(ih->biSizeImage);
					if(ReadFile(hFile, buf, ih->biSizeImage, &dwRead, NULL))
					{
						CloseHandle(hFile);
						return buf;
					}else{
						MessageBox(NULL, L"Could not read the bitmap file.", L"Error", MB_OK);
					}
				}else{
					MessageBox(NULL, L"The ReadFile function failed.", L"Error", MB_OK);
				}
				CloseHandle(hFile);
			}else{
				MessageBox(NULL, L"Cannot open this file", L"Error", MB_OK);
			}
		}else{
			MessageBox(NULL, L"This is not a bitmap file.", L"Warning", MB_OK);
		}
	}

	return NULL;
}
