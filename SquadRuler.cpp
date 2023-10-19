#include "stdafx.h"
#include "ScreenCapture.h"

#define Map1 "RTSS_Ruler_Overlay"
#define Map2 "RTSS_Ruler_Placeholder"

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI ThreadProc(LPVOID param);
std::string outputstr = "";
std::string mainStr = "";
//CHAR ruler_outtext[256] = "Distance: 0";
HWND hWnd, hPop;
int markType = 2, roleType = 0;
bool popup = false;

void MatchingMethod(const cv::Mat& img, const cv::Mat& templ, cv::Point& matchLoc);

void getLimits(cv::Vec3b& bgrPixel, cv::Scalar& minHSV, cv::Scalar& maxHSV, double thresh = 15);

void crop(cv::Mat& map, cv::Mat& scale, cv::Mat& scaleLine) {
    int x_left = 0, x_right = 0, y_top = 0, y_bottom = 0;
    cv::Mat img = cv::imread("temp/screen.png");
    if (img.size().height == 1080 && img.size().width == 1920) {
        x_left = 741;
        x_right = 1488;
        y_top = 133;
        y_bottom = 880;
    }
    else if (img.size().height == 1440 && img.size().width == 2560) {
        x_left = 1094;
        x_right = 2203;
        y_top = 251;
        y_bottom = 1360;
    }
    else if (img.size().height == 1440 && img.size().width == 3440) {
        x_left = 1800;
        x_right = 3440;
        y_top = 133;
        y_bottom = 1306;
    }
    else if (img.size().height == 2160 && img.size().width == 3840) {
        x_left = 1544;
        x_right = 3274;
        y_top = 330;
        y_bottom = 2060;
    }
    map = img(cv::Range(y_top, y_bottom), cv::Range(x_left, x_right));
    //imwrite("temp/map.png", map);

    x_left = 1534; x_right = 1575; y_top = 1060; y_bottom = 1075;
    scale = map(cv::Range(y_top, y_bottom), cv::Range(x_left, x_right));
    cv::cvtColor(scale, scale, cv::COLOR_BGR2GRAY);
    //imwrite("temp/scale.png", scale);
    cv::inRange(scale, 240, 255, scale);

    x_left = 776; x_right = 1575; y_top = 1082; y_bottom = 1083;
    scaleLine = map(cv::Range(y_top, y_bottom), cv::Range(x_left, x_right));
    cv::cvtColor(scaleLine, scaleLine, cv::COLOR_BGR2GRAY);
    cv::inRange(scaleLine, 0, 0, scaleLine);
    //imwrite("temp/scaleLine.png", scaleLine);
    return;
}

bool compForMatch(const std::pair<double, double>& T1, const std::pair<double, double>& T2) {
    return T1.first < T2.first;
}

double getSimilarity(const cv::Mat& A, const cv::Mat& B) {
    if (A.rows > 0 && A.rows == B.rows && A.cols > 0 && A.cols == B.cols) {
        // Calculate the L2 relative error between images.
        double errorL2 = cv::norm(A, B, cv::NORM_L2);
        // Convert to a reasonable scale, since L2 error is summed across all pixels of the image.
        double similarity = errorL2 / (double)(A.rows * A.cols);
        return similarity;
    }
}

bool enterFullscreen(HWND hwnd, int fullscreenWidth, int fullscreenHeight, int colourBits, int refreshRate) {
    DEVMODE fullscreenSettings;
    bool isChangeSuccessful;
    RECT windowBoundary;
    EnumDisplaySettings(NULL, 0, &fullscreenSettings);
    fullscreenSettings.dmPelsWidth = fullscreenWidth;
    fullscreenSettings.dmPelsHeight = fullscreenHeight;
    fullscreenSettings.dmBitsPerPel = colourBits;
    fullscreenSettings.dmDisplayFrequency = refreshRate;
    fullscreenSettings.dmFields = DM_PELSWIDTH |
        DM_PELSHEIGHT |
        DM_BITSPERPEL |
        DM_DISPLAYFREQUENCY;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, fullscreenWidth, fullscreenHeight, SWP_SHOWWINDOW);
    isChangeSuccessful = ChangeDisplaySettings(&fullscreenSettings, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL;
    ShowWindow(hwnd, SW_MAXIMIZE);
    return isChangeSuccessful;
}

void SetSplashImage(HWND hwndSplash, HBITMAP hbmpSplash)
{
    // get the size of the bitmap
    BITMAP bm;
    GetObject(hbmpSplash, sizeof(bm), &bm);
    SIZE sizeSplash = { bm.bmWidth, bm.bmHeight };
    // get the primary monitor's info
    POINT ptZero = { 0 };
    HMONITOR hmonPrimary = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorinfo = { 0 };
    monitorinfo.cbSize = sizeof(monitorinfo);
    GetMonitorInfo(hmonPrimary, &monitorinfo);
    // center the splash screen in the middle of the primary work area
    const RECT& rcWork = monitorinfo.rcWork;
    POINT ptOrigin;
    ptOrigin.x = 0;
    ptOrigin.y = rcWork.top + (rcWork.bottom - rcWork.top - sizeSplash.cy) / 2;
    // create a memory DC holding the splash bitmap
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcMem, hbmpSplash);
    // use the source image's alpha channel for blending
    BLENDFUNCTION blend = { 0 };
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    // paint the window (in the right location) with the alpha-blended bitmap
    UpdateLayeredWindow(hwndSplash, hdcScreen, &ptOrigin, &sizeSplash,
        hdcMem, &ptZero, RGB(0, 0, 0), &blend, ULW_ALPHA);
    // delete temporary objects
    SelectObject(hdcMem, hbmpOld);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	SetProcessDPIAware();
	initSrc();

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	LPCWSTR szWindowClass = TEXT("SQUADRULER");
    LPCWSTR szTitle = TEXT("Squad Ruler");

    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIcon = NULL;
    wcex.hIconSm = NULL;

    if (!RegisterClassExW(&wcex))
        return FALSE;

    /*HWND*/ hWnd = CreateWindowW(szWindowClass, szTitle, WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        425, 384,//GetSystemMetrics(SM_CYMIN) + 230,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

    CreateWindowW(
        TEXT("EDIT"),
        TEXT("Distance: 0\r\nScale: 0"),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_READONLY,
        2, 2, 150, 44, hWnd, (HMENU)1, hInstance, NULL);

    CreateWindowW(
        TEXT("BUTTON"), TEXT("Always on top"),
        WS_CHILD | WS_VISIBLE | WS_BORDER | BS_CENTER | BS_VCENTER | BS_AUTOCHECKBOX,
        2, 74, 150, 20, hWnd, (HMENU)9, hInstance, NULL);

    CreateWindowW(
        TEXT("BUTTON"), TEXT("Pop-Up"),
        WS_CHILD | WS_VISIBLE | WS_BORDER | BS_CENTER | BS_VCENTER | BS_AUTOCHECKBOX,
        2, 96, 150, 20, hWnd, (HMENU)10, hInstance, NULL);

    TCHAR marksType[3][11] =
    {
        TEXT("Squad mark"), TEXT("B mark"), TEXT("C mark")
    };

    TCHAR roleTypes[13][13] =
    {
        TEXT("crewman"), TEXT("engineer"), TEXT("gp"), TEXT("hmg"), TEXT("lat"), TEXT("lmg"), TEXT("medic"), TEXT("rifleman"), TEXT("sniper"), TEXT("squadCrewman"), TEXT("squadLeader"), TEXT("squadPilot"), TEXT("tandem")
    };

    HWND hWndComboBox = CreateWindowW(WC_COMBOBOX, TEXT("Select target mark:"),
        CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
        2, 48, 150, 240, hWnd, (HMENU)7, hInstance,
        NULL);

    HWND hWndComboBox2 = CreateWindowW(WC_COMBOBOX, TEXT("Select your role:"),
        CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
        2, 118, 150, 240, hWnd, (HMENU)8, hInstance,
        NULL);

    TCHAR A[16];
    int  k = 0;

    memset(&A, 0, sizeof(A));
    for (k = 0; k < 3; ++k)
    {
        wcscpy_s(A, sizeof(A) / sizeof(TCHAR), (TCHAR*)marksType[k]);

        // Add string to combobox.
        SendMessage(hWndComboBox, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)A);
    }
    SendMessage(hWndComboBox, CB_SETCURSEL, (WPARAM)2, (LPARAM)0);
    for (k = 0; k < 13; ++k)
    {
        wcscpy_s(A, sizeof(A) / sizeof(TCHAR), (TCHAR*)roleTypes[k]);

        // Add string to combobox.
        SendMessage(hWndComboBox2, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)A);
    }
    SendMessage(hWndComboBox2, CB_SETCURSEL, (WPARAM)2, (LPARAM)0);

	hPop = CreateWindowW(szWindowClass, NULL,
		WS_DLGFRAME | WS_POPUP,
		1660, 1260, 130, 34, NULL, NULL, hInstance, NULL);
	//SetWindowPos(hPop, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	CreateWindowW(
		TEXT("EDIT"),
		TEXT("0"),
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_READONLY | WS_EX_TOPMOST,
		2, 2, 120, 24, hPop, (HMENU)0, hInstance, NULL);

	//ShowWindow(hPop, SW_SHOWNOACTIVATE);
	//SetWindowPos(hPop, HWND_TOPMOST, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE);

    SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE or SWP_NOSIZE); //HWND_TOPMOST

	DWORD dwThreadId;
	CreateThread(NULL, 0, ThreadProc, NULL, 0, &dwThreadId);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    /*LONG lStyle = GetWindowLong(hWnd, GWL_STYLE);
    lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    SetWindowLong(hWnd, GWL_STYLE, lStyle);
    LONG lExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
    lExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLong(hWnd, GWL_EXSTYLE, lExStyle);
    SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);*/



    //HDC windowHDC = GetDC(NULL);
    //int fullscreenWidth = GetDeviceCaps(windowHDC, DESKTOPHORZRES);
    //int fullscreenHeight = GetDeviceCaps(windowHDC, DESKTOPVERTRES);
    //int colourBits = GetDeviceCaps(windowHDC, BITSPIXEL);
    //int refreshRate = GetDeviceCaps(windowHDC, VREFRESH);
    //enterFullscreen(hWnd, fullscreenWidth, fullscreenHeight, colourBits, refreshRate);


	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
    case WM_COMMAND:
        if (HIWORD(wParam) == CBN_SELCHANGE)
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case 7:
                markType = SendMessage((HWND)lParam, (UINT)CB_GETCURSEL,
                    (WPARAM)0, (LPARAM)0);
                break;
            case 8:
                roleType = SendMessage((HWND)lParam, (UINT)CB_GETCURSEL,
                    (WPARAM)0, (LPARAM)0);
                break;
            }
        }
        else {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case 9:
                if (IsDlgButtonChecked(hWnd, 9)) {
                    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                }
                else {
                    SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                }
                break;
            case 10:
                popup = IsDlgButtonChecked(hWnd, 10);
                if (popup) {
                    SetWindowPos(hPop, HWND_TOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
                else {
                    SetWindowPos(hPop, HWND_TOPMOST, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

                }
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void MatchingMethod(const cv::Mat& img, const cv::Mat& templ, cv::Point& matchLoc)
{
    int match_method = cv::TM_CCOEFF_NORMED;

    cv::Mat result;
    /// Source image to display
    cv::Mat img_display;
    img.copyTo(img_display);

    /// Create the result matrix
    int result_cols = img.cols - templ.cols + 1;
    int result_rows = img.rows - templ.rows + 1;

    result.create(result_rows, result_cols, CV_32FC1);

    /// Do the Matching and Normalize
    matchTemplate(img, templ, result, match_method);
    normalize(result, result, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());

    /// Localizing the best match with minMaxLoc
    double minVal; double maxVal; cv::Point minLoc; cv::Point maxLoc;

    minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc, cv::Mat());

    /// For SQDIFF and SQDIFF_NORMED, the best matches are lower values. For all the other methods, the higher the better
    if (match_method == cv::TM_SQDIFF || match_method == cv::TM_SQDIFF_NORMED)
    {
        matchLoc = minLoc;
    }
    else
    {
        matchLoc = maxLoc;
    }

    /*matchLoc.x += templ.cols / 2;
    matchLoc.y += templ.rows / 2;*/

    /// Show me what you got
    //rectangle(img_display, matchLoc, cv::Point(matchLoc.x + templ.cols, matchLoc.y + templ.rows), cv::Scalar(0,0,255), 2, 8, 0);
    //rectangle(result, matchLoc, cv::Point(matchLoc.x + templ.cols, matchLoc.y + templ.rows), cv::Scalar(0, 0, 0), 2, 8, 0);

    /*cv::circle(img_display, matchLoc, 2, cv::Scalar(0, 0, 255), 1, 8, 0);
    cv::circle(result, matchLoc, 2, cv::Scalar(0, 0, 0), 1, 8, 0);*/
    //imshow(image_window, img_display);
    //imshow(result_window, result);

    return;
}

void getLimits(cv::Vec3b& bgrPixel, cv::Scalar& minHSV, cv::Scalar& maxHSV, double thresh) {

    // Create Mat object from vector since cvtColor accepts a Mat object
    cv::Mat3b bgr(bgrPixel);

    //Convert pixel values to other color spaces.
    cv::Mat3b hsv;
    cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    //Get back the vector from Mat
    cv::Vec3b hsvPixel(hsv.at<cv::Vec3b>(0, 0));

    minHSV = cv::Scalar(hsvPixel.val[0] - thresh, (hsvPixel.val[1] - thresh*4) , (hsvPixel.val[2] - thresh*4) );
    maxHSV = cv::Scalar(hsvPixel.val[0] + thresh, (hsvPixel.val[1] + thresh*4), (hsvPixel.val[2] + thresh*4) );

    return;
}

DWORD WINAPI ThreadProc(LPVOID param)
{
	char markKey;
	{
		std::ifstream iFile("config.ini");
		std::string str;
		std::getline(iFile, str);
		if (str == "") {
			markKey = 0x58;
		}
        else if (str == "X1") {
			markKey = 0x05;
		}
		else if (str == "X2") {
			markKey = 0x06;
		}
		else {
			markKey = VkKeyScanA(str[0]);
		}

	}

    //Инициализация программы
    cv::Mat imgHSV, img, maskHSV, maskCrop, scale, scaleLine, templ;
    cv::Scalar minHSV, maxHSV;
    std::vector<cv::Mat> icons;
    std::vector<std::pair<cv::Mat, double>> scaleTemples;
    cv::Point myMarkPos, targetPos, point;
    cv::Vec3b myMarkColor(8, 225, 225), squadColor(0, 255, 0), BColor(157, 108, 159), CColor(130, 170, 90);
    double realScale, virtScale;
    for (const auto& entry : std::filesystem::directory_iterator("icons/")) {
        icons.insert(icons.end(), cv::imread(entry.path().string(), cv::IMREAD_GRAYSCALE));
    }
    for (const auto& entry : std::filesystem::directory_iterator("scales/")) {
        scaleTemples.insert(scaleTemples.end(), std::pair<cv::Mat, double>(cv::imread(entry.path().string(), cv::IMREAD_GRAYSCALE), std::stod(entry.path().filename().string())));
    }
    templ = cv::imread("targets/shieldGray.png", cv::IMREAD_GRAYSCALE);
	while (TRUE) {
        if (GetAsyncKeyState(markKey)) {
            take_screenshot("temp/screen.png");
            //Обрезка и загрузка изображения
            crop(img, scale, scaleLine);
            cvtColor(img, imgHSV, cv::COLOR_BGR2HSV);

            //Нахождение координат игрока
            getLimits(myMarkColor, minHSV, maxHSV, 15);
            cv::inRange(imgHSV, minHSV, maxHSV, maskHSV);

            double sim, minSim = 1000;
            /*for (const auto& icon : icons) {
                MatchingMethod(maskHSV, icon, point);
                maskCrop = maskHSV(cv::Range(point.y, point.y + 20), cv::Range(point.x, point.x + 19));
                sim = getSimilarity(maskCrop, icon);
                if (sim < minSim) {
                    myMarkPos = point;
                    minSim = sim;
                }
            }*/

            MatchingMethod(maskHSV, icons[roleType], myMarkPos);
             myMarkPos.x += 9; myMarkPos.y += 10;
            //myMarkPos.x = 821; myMarkPos.y = 640;
            //cv::imwrite("temp.chosenRole.png", icons[roleType]);

            cv::cvtColor(maskHSV, maskHSV, cv::COLOR_GRAY2BGR);
            cv::circle(maskHSV, myMarkPos, 4, cv::Vec3b(0, 255, 255), -1);
            cv::imwrite("temp/maskMy.png", maskHSV);
            cv::cvtColor(maskHSV, maskHSV, cv::COLOR_BGR2GRAY);

            //Нахождение координат цели
            switch (markType) {
            case 1:
                getLimits(BColor, minHSV, maxHSV, 15);
                break;
            case 2:
                getLimits(CColor, minHSV, maxHSV, 15);
                break;
            default:
                getLimits(squadColor, minHSV, maxHSV, 15);
                break;
            }
            
            cv::inRange(imgHSV, minHSV, maxHSV, maskHSV);
            cv::imwrite("temp/map.png",maskHSV);
            MatchingMethod(maskHSV, templ, targetPos);
            targetPos.x += templ.cols / 2; targetPos.y += templ.rows / 2;

            cv::cvtColor(maskHSV, maskHSV, cv::COLOR_GRAY2BGR);
            //cv::circle(maskHSV, myMarkPos, 2, cv::Vec3b(0, 255, 255), -1);
            cv::circle(maskHSV, targetPos, 2, cv::Vec3b(0, 0, 255), -1);
            cv::imwrite("temp/mask.png", maskHSV);

            //Нахождение реального масштаба
            minSim = 1000;
            for (const auto& scaleTemple : scaleTemples) {
                sim = getSimilarity(scale, scaleTemple.first);
                if (sim < minSim) {
                    realScale = scaleTemple.second;
                    minSim = sim;
                }
            }

            //Нахождение виртуального масштаба
            virtScale = 0;
            double count = 0;
            for (cv::MatConstIterator_<unsigned char> it = scaleLine.begin<unsigned char>(); it != scaleLine.end<unsigned char>(); ++it) {
                if ((unsigned char)(*it) == 255) {
                    ++count;
                    if (count > virtScale) {
                        virtScale = count;
                    }
                }
                else {
                    count = 0;
                }
            }

            double x = myMarkPos.x - targetPos.x;
            double y = myMarkPos.y - targetPos.y;
            double newdistance = (sqrt(x * x + y * y) * realScale / virtScale);
            mainStr = "Distance: " + std::to_string(newdistance) + "\r\nScale: " + std::to_string(realScale);
            SetDlgItemTextA(hWnd, 1, mainStr.c_str());
            outputstr = std::to_string(newdistance);
            SetDlgItemTextA(hPop, 0, outputstr.c_str());
            /*if (popup) {
                outputstr = std::to_string(newdistance);
                SetDlgItemTextA(hPop, 0, outputstr.c_str());
                SetWindowPos(hPop, HWND_TOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                Sleep(2500);
                SetWindowPos(hPop, HWND_TOPMOST, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }*/
        }
		Sleep(100);
	}
}