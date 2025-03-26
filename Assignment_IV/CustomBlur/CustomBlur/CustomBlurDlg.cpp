
// CustomBlurDlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "CustomBlur.h"
#include "CustomBlurDlg.h"
#include "afxdialogex.h"

#include <locale.h>
#include <atlconv.h>
#include <afxwin.h>
#include <windows.h>
#include <psapi.h>
#include <afx.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace std;

DWORD WINAPI Thread_MainRunning(LPVOID pParam);
DWORD WINAPI Thread_Manage(LPVOID pParam);

DWORD WINAPI Thread_MainRunning(LPVOID pParam)
{
	MainRunningThreadParam* pData = (MainRunningThreadParam*)pParam;
	CCustomBlurDlg* pDlg = pData->pDlg;
	int nThreadNum = pData->nThreadNum;
	int nProcessingType = pData->nProcessingType;
	delete pData;  // 할당한 메모리 해제

	CString str;
	BOOL bRet = TRUE;

	/*
		1. Original Image list 에 있는 image를 읽어온다 (최대 10개정도만...)
		2. Thread를 image 개수만큼 선언한다
		3. 이미지 프로세싱을 진행한다 (opencv -> customblur 순)
		4. 이미지 프로세싱이 끝나는 대로 이미지는 저장한다
	*/

	// 변환 전 이미지 인덱스 번호보다 지금 진입한 스레드 번호가 클 경우
	if (pDlg->getOriginImageCount() <= nThreadNum)
	{
#ifdef DEBUG
		str.Format(_T("[DEBUG] %d 번째 스레드 이미지 없어서 시작 안함 (이 메시지가 발생하면 프로그램에 문제가 있습니다)"), nThreadNum + 1);
		pDlg->AddLog(str, e_LogType_Error);
#endif // DEBUG
		pDlg->m_bAutoRunFlag[nThreadNum] = FALSE;
		return 0;
	}

#ifdef DEBUG
	str.Format(_T("[DEBUG] %d 번째 스레드 시작"), nThreadNum + 1);
	pDlg->AddLog(str, e_LogType_Information);
	// Sleep(100);
#endif // DEBUG

	switch (nProcessingType)
	{
	case e_ProcessingType_OpenCV:
		bRet &= pDlg->RUNOpenCVDLL(nThreadNum); // opencvDLL 함수 실행
		break;

	case e_ProcessingType_Custom:
		bRet &= pDlg->RUNCustomDLL(nThreadNum); // CustomDLL 함수 실행
		break;

	default:
		break;
	}

#ifdef DEBUG
	str.Format(_T("[DEBUG] %d 번째 스레드 끝"), nThreadNum + 1);
	pDlg->AddLog(str);
#endif // DEBUG

	// 레포트 기록
	int nTempNum = nThreadNum + (MAX_THREAD * nProcessingType);

	CReportData reportData = pDlg->GetDataToReport(nThreadNum);
	str.Format(_T("No. %d Thread - Memory Usage : %luMB, Tact Time : %I64dms"), nThreadNum + 1, (ULONG)reportData.dwUsingMemory[nTempNum], reportData.lfTactTime[nTempNum]);		// Log 표시 할 때는 배열 참조 위치가 보정된 위치로 들어가야 함
	pDlg->AddLog(str, e_LogType_Information);

	pDlg->m_bAutoRunFlag[nThreadNum] = FALSE;
	CloseHandle(pDlg->m_hThreadEndEvents[nThreadNum]);

	return 0;
}

DWORD WINAPI Thread_Manage(LPVOID pParam)
{
	ManageThreadParam* pManageData = (ManageThreadParam*)pParam;
	CCustomBlurDlg* pDlg = pManageData->pDlg;

	BOOL bDEMOMode = pManageData->bDEMOMode;
	int i = 0;
	int nCurrentSequence = 0;
	const int nMaxSequence = pManageData->nMaxSequence;
	CString str;

	delete pManageData;
	pDlg->DialogLockORUnlock(FALSE);			// 다이얼로그 비활성화!!
	pDlg->ResetReportData();
	pDlg->ResetAutorunFlag();
	pDlg->SetCurTime(e_TimeType_StartTime);

	while (nCurrentSequence < nMaxSequence)
	{
		int nProcessingType = e_ProcessingType_OpenCV;
		int nRun = pDlg->GetImageCount();		// m_list_originImage에 추가할 때 THREAD MAX 초과하여 이미지 추가할 수 없게 예외처리 되어 있기 때문에 이렇게 해도 될 듯
		pDlg->m_nMaxThreads = std::max(pDlg->m_nMaxThreads, nRun);

		// MainRunningThread Init
		for (i = 0; i < nRun; i++)
		{
			MainRunningThreadParam* pThreadData = new MainRunningThreadParam;

			pThreadData->pDlg = pDlg;
			pThreadData->nThreadNum = i;
			pThreadData->nProcessingType = pDlg->GetProcessingType();
			if (bDEMOMode) pThreadData->nProcessingType = nCurrentSequence;			// 데모모드일 경우, 프로세싱 타입을 현재 시퀀스에 맞게 강제 변환

			pDlg->m_hThreadEndEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
			pThreadData->hThreadEndEvent = pDlg->m_hThreadEndEvents[i];

			pDlg->m_bAutoRunFlag[i] = TRUE;
			pDlg->m_hMainRunningThread[i] = CreateThread(NULL, 0, Thread_MainRunning, pThreadData, 0, NULL);

			DWORD_PTR affinityMask = 1 << (i % pDlg->m_info.dwNumberOfProcessors);
			SetThreadAffinityMask(pDlg->m_hMainRunningThread[i], affinityMask);

#ifdef DEBUG
			str.Format(_T("[%d/%d] 번째 시퀀스 %d 번째 스레드 생성"), nCurrentSequence + 1, nMaxSequence, i + 1);
			pDlg->AddLog(str, e_LogType_Information);
#endif // DEBUG
		}

		while (TRUE)
		{
			BOOL bAllFalse = TRUE;
			for (int i = 0; i < MAX_THREAD; i++)
			{
				if (pDlg->m_bAutoRunFlag[i])
				{
					bAllFalse = FALSE;
					break;
				}
			}

			if (bAllFalse)
			{
				nCurrentSequence++;
				break;
			}

			Sleep(10);
		}

#ifdef DEBUG
		str.Format(_T("%d 번째 시퀀스 완료..."), nCurrentSequence);
		pDlg->AddLog(str, e_LogType_Information);
#endif // DEBUG
	}

	pDlg->SetCurTime(e_TimeType_EndTime);
	pDlg->WriteReport();
	pDlg->AddLog(_T("작업이 종료되었습니다."), e_LogType_Complete);
	pDlg->DialogLockORUnlock(TRUE);			// 다이얼로그 활성화

	return 0;
}

DWORD GetMemoryUsage()
{
	PROCESS_MEMORY_COUNTERS pmc;

	if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
		return static_cast<DWORD>(pmc.WorkingSetSize / 1024 / 1024);

	return 0;
}

// CCustomBlurDlg 대화 상자


CCustomBlurDlg::CCustomBlurDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CUSTOMBLUR_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CCustomBlurDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_LOG, m_list_log);
	DDX_Control(pDX, IDC_LIST_ORIGINIMG, m_list_originImage);
}

BEGIN_MESSAGE_MAP(CCustomBlurDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_IMAGELOAD, &CCustomBlurDlg::OnBnClickedButtonImageload)
	ON_BN_CLICKED(IDC_BUTTON_IMAGEDELETE, &CCustomBlurDlg::OnBnClickedButtonImagedelete)
	ON_BN_CLICKED(IDC_BUTTON_DEMORUN, &CCustomBlurDlg::OnBnClickedButtonDemorun)
	ON_BN_CLICKED(IDC_BUTTON_RUN, &CCustomBlurDlg::OnBnClickedButtonRun)
	ON_BN_CLICKED(IDC_BUTTON_RUN2, &CCustomBlurDlg::OnBnClickedButtonRun2)
	ON_BN_CLICKED(IDC_BUTTON_MANUAL, &CCustomBlurDlg::OnBnClickedButtonManualRun)
	ON_BN_CLICKED(IDC_BUTTON_MANUAL2, &CCustomBlurDlg::OnBnClickedButtonManualRun2)
	ON_BN_CLICKED(IDC_BUTTON_GRAY, &CCustomBlurDlg::OnBnClickedButtonGray)
	ON_BN_CLICKED(IDC_BUTTON_COMPARE, &CCustomBlurDlg::OnBnClickedButtonCompare)
	ON_BN_CLICKED(IDCANCEL, &CCustomBlurDlg::OnBnClickedCancel)
END_MESSAGE_MAP()


// CCustomBlurDlg 메시지 처리기

BOOL CCustomBlurDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	InitDLL();
	Init();

	GetSystemInfo(&m_info);

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CCustomBlurDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CCustomBlurDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CCustomBlurDlg::InitDLL()
{
	// dll 불러오는 구문
	int nDLLLoadFlag = e_DLLLoadFlag_OK;

	// DLL 로딩
	m_hModule_CustomDLL = LoadLibrary(_T("CustomBlurDLL.dll"));
	m_hModule_OpenCVDLL = LoadLibrary(_T("OpenCVBlurDLL.dll"));

	if (!m_hModule_CustomDLL)		nDLLLoadFlag |= e_DLLLoadFlag_CustomDLL_LoadError;
	if (!m_hModule_OpenCVDLL)		nDLLLoadFlag |= e_DLLLoadFlag_OpenCVWarpDLL_LoadError;

	if (nDLLLoadFlag != e_DLLLoadFlag_OK)
	{
		CString str;
		switch (nDLLLoadFlag)
		{
		case e_DLLLoadFlag_UNKNOWN:						str.Format(_T("알 수 없는 DLL 로드 오류입니다."));										break;
		case e_DLLLoadFlag_CustomDLL_LoadError:			str.Format(_T("Custom DLL이 정상적으로 로드되지 않았습니다."));							break;
		case e_DLLLoadFlag_OpenCVWarpDLL_LoadError:		str.Format(_T("OpenCV Wrap DLL이 정상적으로 로드되지 않았습니다."));					break;
		case e_DLLLoadFlag_ALL_LoadError:				str.Format(_T("Custom DLL 및 OpenCV Wrap DLL이 정상적으로 로드되지 않았습니다.."));		break;
		default:										str.Format(_T("알 수 없는 DLL 로드 오류입니다. (default Flag)"));						break;
		}

		str.Append(_T("\n프로그램을 종료합니다."));
		AfxMessageBox(str);

		if (m_hModule_CustomDLL != NULL)		FreeLibrary(m_hModule_CustomDLL);
		if (m_hModule_OpenCVDLL != NULL)		FreeLibrary(m_hModule_OpenCVDLL);

		AfxGetMainWnd()->PostMessage(WM_CLOSE);
	}
}

void CCustomBlurDlg::Init()
{
	/*
		1. ini 파일 로드 시도
		2. ini 파일 있으면 -> 해당 옵션을 적용
		3. ini 파일 없으면 -> 기본값 옵션을 적용한 파일을 생성
	*/

	CString str;
	m_option = LoadOptionFile();

	if (m_option.bInitFlag == FALSE)
	{
		m_bInitFlag &= WriteFile(OPTION_FILE, e_File_Init);
		m_bInitFlag &= CreateFolder(m_option.strImageFileDir);
		m_bInitFlag &= CreateFolder(m_option.strSaveImageFileDir);
	}

	if (m_bInitFlag == FALSE)
	{
		AfxMessageBox(_T("프로그램을 종료합니다."));
		AfxGetMainWnd()->PostMessage(WM_CLOSE);
	}

	m_logManager.Start();

	str.Format(_T("%d"), m_option.nKernelSize);
	SetDlgItemText(IDC_EDIT_KERNELSIZE, str);
	ReadImage(m_option.strImageFileDir);
}

void CCustomBlurDlg::OnBnClickedButtonImageload()
{
	int nNum = m_list_originImage.GetCount();
	if (nNum >= MAX_THREAD)
	{
		CString str;
		str.Format(_T("추가 가능한 파일 수를 초과하였습니다. 허용 가능한 파일 수 : %d"), MAX_THREAD);
		AddLog(str, e_LogType_Error);
		return;
	}

	CFileDialog fileDlg(TRUE, NULL, NULL, OFN_READONLY, _T("image file(*.jpg;*.jpeg;*.bmp;*.png;)|*.jpg;*.jpeg;*.bmp;*.png;|All Files(*.*)|*.*||"));
	if (fileDlg.DoModal() == IDOK)
	{
		CString strFileName = fileDlg.GetPathName();

		if (VerifyImageIsGrayscale(strFileName) == TRUE)
			AddOriginalImage(strFileName);
	}
}

void CCustomBlurDlg::ReadImage(CString strPathName)
{
	/*
		1. 경로 내 리스트를 읽음
		2. 읽으면서 jpg, bmp, png만 필터링
		3. list에 추가하면 됨
	*/

	if (!strPathName.IsEmpty() && strPathName[0] == _T('.'))		// 경로 주소가 상대경로 (.\\~~~~) 일 경우 절대경로로 변경
	{
		strPathName.Delete(0, 1);
		strPathName.Format(_T("%s%s"), GetDirectory(), strPathName);
	}

	int nMaxFileCount = 0;
	CString strOriginFileName;								// 파일명
	CString strSearch = strPathName + _T("\\*.*");			// 찾는파일의유형
	CFileFind ff;
	BOOL bFindfile = ff.FindFile(strSearch);
	BOOL bTempFlag = TRUE;
	CString str;

	while (bFindfile)
	{
		bFindfile = ff.FindNextFile();

		if (ff.IsDots() || ff.IsDirectory())
			continue;

		strOriginFileName = ff.GetFilePath();

		CString strDirectory, strFileName, strExtension;
		SplitFilePath(strOriginFileName, strDirectory, strFileName, strExtension);

		strExtension.MakeLower();

		if (strExtension == _T(".jpg") || strExtension == _T(".jpeg") || strExtension == _T(".bmp") || strExtension == _T(".png"))
		{
			CString str;
			str.Format(_T("%s"), strOriginFileName);
			if (VerifyImageIsGrayscale(str) == TRUE)
			{
				if (nMaxFileCount >= MAX_THREAD)
				{
					if (bTempFlag)
					{
						str.Format(_T("추가 가능한 파일 수를 초과하였습니다. 허용 가능한 파일 수 : %d"), MAX_THREAD);
						AddLog(str, e_LogType_Error);
						bTempFlag &= FALSE;
					}

					str.Format(_T("[%s] 이미지 추가에 실패하였습니다."), strOriginFileName);
					AddLog(str, e_LogType_Error);
					continue;
				}

				AddOriginalImage(str);
				nMaxFileCount++;
			}
		}
	}

	ff.Close();
}

void CCustomBlurDlg::SplitFilePath(const CString& strInput, CString& strOutputDirectory, CString& strOutputFileName, CString& strOutputExtension)
{
	// 마지막 역슬래시 위치 찾기
	int posBackslash = strInput.ReverseFind(_T('\\'));
	CString filePart;
	if (posBackslash == -1)
	{
		// 역슬래시가 없으면 전체 경로가 파일명과 확장자를 포함한 문자열임
		strOutputDirectory = _T("");
		filePart = strInput;
	}
	else
	{
		strOutputDirectory = strInput.Left(posBackslash);
		filePart = strInput.Mid(posBackslash + 1);
	}

	// filePart에서 마지막 '.' 위치를 찾아 파일명과 확장자를 분리
	int posDot = filePart.ReverseFind(_T('.'));
	if (posDot == -1)
	{
		// 확장자가 없는 경우
		strOutputFileName = filePart;
		strOutputExtension = _T("");
	}
	else
	{
		strOutputFileName = filePart.Left(posDot);
		strOutputExtension = filePart.Mid(posDot);
	}
}

BOOL CCustomBlurDlg::VerifyImageIsGrayscale(CString strFileName)
{
	BOOL bRet = TRUE;
	CString str;

	CT2CA pszConvertedAnsiString(strFileName);
	std::string stdPath(pszConvertedAnsiString);
	cv::Mat image = cv::imread(stdPath, cv::IMREAD_UNCHANGED);
	// image.copyTo(m_srcMat);

	int nCh = image.channels();
	int nty = image.type();

	if (image.empty())
	{
		str.Format(_T("%s 이미지가 정상적으로 로드되지 않았습니다(이미지가 비어 있습니다)."), strFileName);
		AddLog(str, e_LogType_Error);
		bRet &= FALSE;
	}

	else if (image.channels() != 1)
	{
		str.Format(_T("%s 이미지는 흑백이 아닙니다."), strFileName);
		AddLog(str, e_LogType_Error);
		bRet &= FALSE;
	}

	return bRet;
}

BOOL CCustomBlurDlg::WriteFile(CString str, const int nCase)
{
	BOOL bRet = TRUE;
	setlocale(LC_ALL, "korean");
	CStdioFile file;
	CString strFileName = _T("");

	switch (nCase)
	{
	case e_File_Init:		strFileName.Format(OPTION_FILE);	break;
	case e_File_Log:		strFileName.Format(LOG_FILE);		break;

	default:
		bRet &= FALSE;
		AfxMessageBox(_T("Function:WriteLog, Switch:nCase Error"));
		break;
	}

	BOOL bFileFindFlag = FALSE;
	CFileFind ff;

	if (ff.FindFile((LPCTSTR)strFileName))		bFileFindFlag = TRUE;
	else										bFileFindFlag = FALSE;

	switch (nCase)
	{
	case e_File_Init:			// 여기 들어오는경우는 .ini 파일이 없어서 쓰는경우밖에 없음...
		if (bFileFindFlag)
		{
			AfxMessageBox(_T("Function:WriteLog, Switch:nCase, bFileFindFlag = true Error"));		// 논리적으로 여기를 들어올수없음
			bRet &= FALSE;
		}

		else
		{
			CString str;

			if (bRet && file.Open(strFileName, CFile::modeCreate | CFile::modeWrite | CFile::typeText))
			{
				file.WriteString(_T("imageFileDir=.\\images\n"));
				file.WriteString(_T("kernelSize=21\n"));
				file.WriteString(_T("saveImageFileDir=.\\saveimages\n"));
				file.Close();
			}

			else
			{
				bRet &= FALSE;
				str.Format(_T("%s 파일 생성에 실패했습니다."), strFileName);
				AfxMessageBox(str);
			}
		}

		break;

	case e_File_Log:

		str.Append(_T("\n"));
		if (bFileFindFlag)	bRet &= file.Open(strFileName, CFile::modeWrite | CFile::typeText);
		else				bRet &= file.Open(strFileName, CFile::modeCreate | CFile::modeWrite | CFile::typeText);

		if (!bRet)
		{
			AfxMessageBox(_T("Log File 열기에 실패하였습니다."));
			break;
		}

		file.SeekToEnd();

		// 굳이 AddLog에서 시간을 주는데 여기에서 할 필요가?
		/*
		SYSTEMTIME cur_time;
		GetLocalTime(&cur_time);
		str.Format(_T("[%04d-%02d-%02d %02d:%02d:%02d] %s"), cur_time.wYear, cur_time.wMonth, cur_time.wDay, cur_time.wHour, cur_time.wMinute, cur_time.wSecond, str);
		*/
		file.WriteString(str);
		file.Close();
		break;

	default:
		bRet &= FALSE;
		AfxMessageBox(_T("Function:WriteLog, Switch:nCase Error"));
		break;
	}

	return bRet;
}

BOOL CCustomBlurDlg::CreateFolder(const CString& strFolderPath)
{
	BOOL bRet = TRUE;

	if (CreateDirectory(strFolderPath, NULL) == FALSE && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		DWORD dwError = GetLastError();
		CString str;
		str.Format(_T("폴더 생성에 실패했습니다.\n폴더: %s\n오류 코드: %d"), strFolderPath, dwError);
		AfxMessageBox(str);
		bRet &= FALSE;
	}

	return bRet;
}

CProgramOption CCustomBlurDlg::LoadOptionFile()
{
	CProgramOption options;
	CString strFileName = OPTION_FILE;
	setlocale(LC_ALL, "korean");
	CStdioFile file;

	if (!file.Open(strFileName, CFile::modeRead | CFile::typeText))
	{
		CString str;
		str.Format(_T("%s 파일이 존재하지 않습니다. 기본값으로 파일을 생성합니다."), strFileName);
		AfxMessageBox(str);
		return options;
	}

	else
	{
		CString strLine;
		while (file.ReadString(strLine))
		{
			strLine.Trim();
			if (strLine.IsEmpty())
				continue;

			int posEqual = strLine.Find(_T('='));
			if (posEqual == -1)
				continue;

			CString key = strLine.Left(posEqual);
			CString value = strLine.Mid(posEqual + 1);
			key.Trim();
			value.Trim();

			if (key.CompareNoCase(_T("imageFileDir")) == 0)				options.strImageFileDir = value;
			else if (key.CompareNoCase(_T("kernelSize")) == 0)			options.nKernelSize = _ttoi(value);
			else if (key.CompareNoCase(_T("saveImageFileDir")) == 0)	options.strSaveImageFileDir = value;
		}

		options.bInitFlag = TRUE;
		file.Close();
	}

	return options;
}

CString CCustomBlurDlg::GetDirectory()
{
	TCHAR szBuffer[MAX_PATH];
	DWORD dwRet = ::GetCurrentDirectory(MAX_PATH, szBuffer);
	if (dwRet == 0 || dwRet >= MAX_PATH)
		return CString();

	CString strCurrentDir(szBuffer);
	return strCurrentDir;
}

void CCustomBlurDlg::AddLog(CString strLog, int nLogType)
{
	BOOL bLogFileWriteFlag = TRUE;
	CString strType;
	SYSTEMTIME cur_time;
	GetLocalTime(&cur_time);

	switch (nLogType)
	{
	case e_LogType_NotWrite:
		break;

	case e_LogType_Information:
		strType.Format(_T("[INFO] "));
		break;

	case e_LogType_Complete:
		strType.Format(_T("[COMPLETE] "));
		break;

	case e_LogType_Caution:
		strType.Format(_T("[CAUTION] "));
		break;

	case e_LogType_Error:
		strType.Format(_T("[ERROR] "));
		break;

	default:
		break;
	}

	strLog.Format(_T("[%04d-%02d-%02d %02d:%02d:%02d] %s%s"), cur_time.wYear, cur_time.wMonth, cur_time.wDay, cur_time.wHour, cur_time.wMinute, cur_time.wSecond, strType, strLog);
	m_list_log.AddString(strLog);

	// 로그파일은 남겨야할때만 남겨야함
	if (bLogFileWriteFlag)
		m_logManager.AddLog(strLog);
	//WriteFile(strLog, e_File_Log);

	m_list_log.SetTopIndex(m_list_log.GetCount() - 1);
	m_list_log.SetCurSel(m_list_log.GetCount() - 1);
}

void CCustomBlurDlg::AddOriginalImage(CString strPath)
{
	// 중복제외
	int nFound = m_list_originImage.FindStringExact(-1, strPath);
	if (nFound != LB_ERR)
		AfxMessageBox(_T("이미 등록한 파일입니다."));

	else
		m_list_originImage.AddString(strPath);
}

void CCustomBlurDlg::OnBnClickedButtonImagedelete()
{
	if (m_list_originImage.GetCount() <= 0)
		return;

	int nSel = m_list_originImage.GetCurSel();
	if (nSel == 0xffffffff)
		return;

	CString str;
	m_list_originImage.GetText(nSel, str);
	str.Format(_T("(%s) 이미지를 리스트에서 제거하였습니다."), str);
	AddLog(str);

	m_list_originImage.DeleteString(nSel);		// 얘를 제일나중에 해야함... 안그러면 매개변수 에러발생함
}

// 이미지를 Grayscale로 변환
void CCustomBlurDlg::OnBnClickedButtonGray()
{
	CFileDialog fileDlg(TRUE, NULL, NULL, OFN_READONLY, _T("image file(*.jpg;*.jpeg;*.bmp;*.png;)|*.jpg;*.jpeg;*.bmp;*.png;|All Files(*.*)|*.*||"));
	if (fileDlg.DoModal() == IDOK)
	{
		CString strOriginFileName = fileDlg.GetPathName();		// 전체경로

		CString strDirectory, strFileName, strExtension;
		SplitFilePath(strOriginFileName, strDirectory, strFileName, strExtension);

		// 전체 경로 변환 후 import
		CT2CA pszConvertedAnsiString(strOriginFileName);
		std::string stdPath(pszConvertedAnsiString);
		cv::Mat image = cv::imread(stdPath, cv::IMREAD_COLOR);

		cv::Mat image_Gray;
		cv::cvtColor(image, image_Gray, cv::COLOR_BGR2GRAY);

		// 결과물 저장 파일명 변환
		CString strOutput;
		strOutput.Format(_T("%s\\GRAY_%s%s"), strDirectory, strFileName, strExtension);
		CT2CA pszConvertedAnsiString2(strOutput);
		std::string stdFile(pszConvertedAnsiString2);

		// 이미지 저장
		CString str;
		if (!cv::imwrite(stdFile, image_Gray))		str.Format(_T("[%s] GrayScale 이미지 저장에 실패하였습니다."), strOutput);
		else										str.Format(_T("[%s] GrayScale 이미지 저장에 성공하였습니다."), strOutput);

		AddLog(str);
	}
}

void CCustomBlurDlg::OnBnClickedButtonDemorun()
{
	BOOL bDEMOMode = TRUE;
	BeginThread(bDEMOMode);
}

void CCustomBlurDlg::OnBnClickedButtonRun()
{
	SetProcessingType(e_ProcessingType_OpenCV);
	BeginThread();
}

void CCustomBlurDlg::OnBnClickedButtonRun2()
{
	SetProcessingType(e_ProcessingType_Custom);
	BeginThread();
}

void CCustomBlurDlg::BeginThread(BOOL bDEMOMODE)
{
	// 이미지가 하나도 없으면 예외처리 해야함
	if (m_list_originImage.GetCount() <= 0)
	{
		AddLog(_T("이미지를 추가하여 주십시오."), e_LogType_Error);
		return;
	}

	if (m_hMainRunningThread == NULL)	m_hMainRunningThread = new HANDLE[m_nThreads];

	// Manage Thread Init (Main Running Thread는 Manage Thread 에서 Init 한다)
	ManageThreadParam* pThreadData = new ManageThreadParam;
	pThreadData->pDlg = this;

	if (bDEMOMODE == TRUE)
	{
		pThreadData->nMaxSequence = 2;
		pThreadData->bDEMOMode = TRUE;
	}
	else
	{
		pThreadData->nMaxSequence = 1;
		pThreadData->bDEMOMode = FALSE;
	}

	m_hManageThread = CreateThread(NULL, 0, Thread_Manage, pThreadData, 0, NULL);

	DWORD_PTR affinityMask = 1 << (1 % m_info.dwNumberOfProcessors);
	SetThreadAffinityMask(m_hManageThread, affinityMask);
}

// OpenCV dll 사용 -> 리스트에서 선택한 이미지 1개만 변환 (수동 변환)
void CCustomBlurDlg::OnBnClickedButtonManualRun()
{
	BOOL bRet = TRUE;

	// 리스트에서 이미지 선택
	int nSel = m_list_originImage.GetCurSel();

	if (nSel == 0xffffffff)
	{
		AddLog(_T("리스트에서 이미지가 선택되지 않았습니다. 리스트에서 이미지를 선택하여 주십시오."), e_LogType_Error);
		return;
	}

	bRet &= RUNOpenCVDLL(nSel);
}

// custom dll 사용 -> 리스트에서 선택한 이미지 1개만 변환 (수동 변환)
void CCustomBlurDlg::OnBnClickedButtonManualRun2()
{
	BOOL bRet = TRUE;

	// 리스트에서 이미지 선택
	int nSel = m_list_originImage.GetCurSel();

	if (nSel == 0xffffffff)
	{
		AddLog(_T("리스트에서 이미지가 선택되지 않았습니다. 리스트에서 이미지를 선택하여 주십시오."), e_LogType_Error);
		return;
	}

	bRet &= RUNCustomDLL(nSel);
}

// ImageObject -> Mat
BOOL CCustomBlurDlg::CopyImageObjectToMat(const ImageObject* obj, cv::Mat& mat)
{
	if (obj == nullptr || obj->data == nullptr || obj->rows <= 0 || obj->cols <= 0)
		return FALSE;

	mat.create(obj->rows, obj->cols, CV_8UC(obj->ch));

	size_t dataSize = static_cast<size_t>(obj->rows) * obj->cols * obj->ch;
	memcpy(mat.data, obj->data, dataSize);

	return TRUE;
}

// Mat -> ImageObject
BOOL CCustomBlurDlg::CopyMatToImageObject(const cv::Mat& mat, ImageObject* obj)
{
	if (mat.empty() || obj == nullptr)
		return FALSE;

	obj->cols = mat.cols;
	obj->rows = mat.rows;
	obj->ch = mat.channels();

	// 전체 바이트 수 계산
	size_t dataSize = mat.total() * mat.elemSize();

	// 새 메모리 할당
	obj->data = new unsigned char[dataSize];
	if (!obj->data)
		return FALSE;

	memcpy(obj->data, mat.data, dataSize);

	return TRUE;
}

BOOL CCustomBlurDlg::RUNOpenCVDLL(int nNum)
{
	BOOL bRet = TRUE;
	CString str, strOriginFileName;

	m_list_originImage.GetText(nNum, strOriginFileName);

	// 이미지를 Mat에다 담는다
	CT2CA pszConvertedAnsiString(strOriginFileName);
	std::string stdPath(pszConvertedAnsiString);
	m_srcMat[nNum] = cv::imread(stdPath, cv::IMREAD_UNCHANGED);

	// 커널 사이즈 체크
	GetDlgItemText(IDC_EDIT_KERNELSIZE, str);
	int nKernelSize = _wtof(str);

	// Timer & Processing Start
	LONGLONG lTimer = StartTimer();

	ImageObject src, dst;

	// dst의 dataSize는 미리 만들어 놓는다 (DLL에서 만들면 터짐)
	size_t dataSize = m_srcMat[nNum].total() * m_srcMat[nNum].elemSize();
	dst.data = new unsigned char[dataSize];

	CopyMatToImageObject(m_srcMat[nNum], &src);			// src에는 opencv의 image를 넣어준다

	CustomImageBlur pBlurFunc_OpenCV = (CustomImageBlur)GetProcAddress(m_hModule_OpenCVDLL, "ImageBlur");
	bRet &= pBlurFunc_OpenCV(&src, &dst, nKernelSize);

	if (bRet == TRUE)
		CopyImageObjectToMat(&dst, m_dstMat[nNum]);

	if (src.data)
	{
		delete[] src.data;
		src.data = nullptr;
	}

	if (dst.data)
	{
		delete[] dst.data;
		dst.data = nullptr;
	}

	if (bRet == FALSE)
	{
		AddLog(_T("OpenCV Wrap DLL을 이용하여 이미지를 변환하는 도중 오류가 발생하였습니다."), e_LogType_Error);
		return FALSE;
	}

	// m_dstMat[nNum] = ProcessingBlur_OpenCV(m_srcMat[nNum], nKernelSize, nKernelSize);		// 외부 dll에서 wrap 하므로 더 이상 사용하지 않습니다

	CString strDirectory, strFileName, strExtension;
	SplitFilePath(strOriginFileName, strDirectory, strFileName, strExtension);

	CString strOutput = m_option.strSaveImageFileDir;
	if (!strOutput.IsEmpty() && strOutput[0] == _T('.'))		// 경로 주소가 상대경로 (.\\~~~~) 일 경우 절대경로로 변경
	{
		strOutput.Delete(0, 1);
		strOutput.Format(_T("%s%s"), GetDirectory(), strOutput);
	}

	strOutput.Format(_T("%s\\BLUR_OPENCV_%s%s"), strOutput, strFileName, strExtension);
	CT2CA pszConvertedAnsiString2(strOutput);
	std::string stdPath2(pszConvertedAnsiString2);

	if (cv::imwrite(stdPath2, m_dstMat[nNum]))
		str.Format(_T("[%s] Blur 이미지 저장에 성공하였습니다."), strOutput);

	else
	{
		str.Format(_T("[%s] Blur 이미지 저장에 실패하였습니다."), strOutput);
		bRet &= FALSE;
	}
	AddLog(str);

	LONGLONG nTemp = EndTimer(lTimer);


	// if (m_bAutoRunFlag[nNum])	str.Format(_T("[AUTO MODE/OPENCV] [No. %d Image] Tact Time : %I64dms"), nNum + 1, nTemp);
	// else							str.Format(_T("Tact Time : %I64dms"), nTemp);

	if (m_bAutoRunFlag[nNum])
	{
		// 자동 변환 일 때는 정보를 기록
		int nTempNum = nNum + (MAX_THREAD * e_ProcessingType_OpenCV);
		str.Format(_T("%s%s"), strFileName, strExtension);
		SetDataToReport(TRUE, nTempNum, str, m_dstMat[nNum].cols, m_dstMat[nNum].rows, GetMemoryUsage(), nTemp);
	}

	else
	{
		str.Format(_T("Tact Time : %I64dms"), nTemp);
		AddLog(str);
	}

	return bRet;
}

BOOL CCustomBlurDlg::RUNCustomDLL(int nNum)
{
	BOOL bRet = TRUE;
	CString str, strOriginFileName;

	m_list_originImage.GetText(nNum, strOriginFileName);

	// 이미지를 Mat에다 담는다
	CT2CA pszConvertedAnsiString(strOriginFileName);
	std::string stdPath(pszConvertedAnsiString);
	m_srcMat[nNum] = cv::imread(stdPath, cv::IMREAD_UNCHANGED);

	// 커널 사이즈 체크
	GetDlgItemText(IDC_EDIT_KERNELSIZE, str);
	int nKernelSize = _wtof(str);

	// Timer & Processing Start
	LONGLONG lTimer = StartTimer();

	// Custom DLL
	ImageObject src, dst;
	CopyMatToImageObject(m_srcMat[nNum], &src);			// src에는 opencv의 image를 넣어준다

	// dst 이미지 크기 선언 ///////////////////////////////////////////////////////////////
	dst.ch = src.ch;
	dst.cols = src.cols;
	dst.rows = src.rows;

	size_t totalSize = static_cast<size_t>(dst.rows) * dst.cols * dst.ch;

	dst.data = new unsigned char[totalSize];
	memset(dst.data, 0xFF, totalSize);
	//////////////////////////////////////////////////////////////////////////////////////

	//BOOL bRet = TestImageBlur(&src, &dst, nKernelSize);
	CustomImageBlur pBlurFunc_Custom = (CustomImageBlur)GetProcAddress(m_hModule_CustomDLL, "ImageBlur");
	bRet &= pBlurFunc_Custom(&src, &dst, nKernelSize);

	if (bRet == TRUE)
		CopyImageObjectToMat(&dst, m_dstMat[nNum]);

	if (src.data)
	{
		delete[] src.data;
		src.data = nullptr;
	}

	if (dst.data)
	{
		delete[] dst.data;
		dst.data = nullptr;
	}

	CString strDirectory, strFileName, strExtension;
	SplitFilePath(strOriginFileName, strDirectory, strFileName, strExtension);

	CString strOutput = m_option.strSaveImageFileDir;
	if (!strOutput.IsEmpty() && strOutput[0] == _T('.'))		// 경로 주소가 상대경로 (.\\~~~~) 일 경우 절대경로로 변경
	{
		strOutput.Delete(0, 1);
		strOutput.Format(_T("%s%s"), GetDirectory(), strOutput);
	}

	strOutput.Format(_T("%s\\BLUR_CUSTOM_%s%s"), strOutput, strFileName, strExtension);
	CT2CA pszConvertedAnsiString2(strOutput);
	std::string stdPath2(pszConvertedAnsiString2);

	if (!cv::imwrite(stdPath2, m_dstMat[nNum]))		str.Format(_T("[%s] Blur 이미지 저장에 실패하였습니다."), strOutput);
	else											str.Format(_T("[%s] Blur 이미지 저장에 성공하였습니다."), strOutput);
	AddLog(str);

	LONGLONG nTemp = EndTimer(lTimer);

	// if (m_bAutoRunFlag[nNum])	str.Format(_T("[AUTO MODE/CUSTOM] [No. %d Image] Tact Time : %I64dms"), nNum + 1, nTemp);
	// else							str.Format(_T("Tact Time : %I64dms"), nTemp);

	if (m_bAutoRunFlag[nNum])
	{
		// 자동 변환 일 때는 정보를 기록
		int nTempNum = nNum + (MAX_THREAD * e_ProcessingType_Custom);
		str.Format(_T("%s%s"), strFileName, strExtension);
		SetDataToReport(TRUE, nTempNum, str, m_dstMat[nNum].cols, m_dstMat[nNum].rows, GetMemoryUsage(), nTemp);
	}

	else
	{
		str.Format(_T("Tact Time : %I64dms"), nTemp);
		AddLog(str);
	}

	return bRet;
}

// 이미지 2개를 비교하는 함수
void CCustomBlurDlg::OnBnClickedButtonCompare()
{
	CString strFile1, strFile2;

	CFileDialog fileDlg(TRUE, NULL, NULL, OFN_READONLY, _T("image file(*.jpg;*.jpeg;*.bmp;*.png;)|*.jpg;*.jpeg;*.bmp;*.png;|All Files(*.*)|*.*||"));
	if (fileDlg.DoModal() == IDOK)
		strFile1 = fileDlg.GetPathName();

	CFileDialog fileDlg2(TRUE, NULL, NULL, OFN_READONLY, _T("image file(*.jpg;*.jpeg;*.bmp;*.png;)|*.jpg;*.jpeg;*.bmp;*.png;|All Files(*.*)|*.*||"));
	if (fileDlg2.DoModal() == IDOK)
		strFile2 = fileDlg2.GetPathName();

	// Early Return -> 이미지가 흑백이 아니면 바로 처리함
	if ((VerifyImageIsGrayscale(strFile1) && VerifyImageIsGrayscale(strFile2)) == FALSE)
		return;

	// opencv를 이용하여 두 파일의 데이터가 같은지 확인
	// 1번 파일
	CT2CA pszConvertedAnsiString(strFile1);
	std::string stdPath(pszConvertedAnsiString);
	cv::Mat image1 = cv::imread(stdPath, cv::IMREAD_UNCHANGED);

	// 2번 파일
	CT2CA pszConvertedAnsiString2(strFile2);
	std::string stdPath2(pszConvertedAnsiString2);
	cv::Mat image2 = cv::imread(stdPath2, cv::IMREAD_UNCHANGED);

	int nRet = CheckImageCompare(image1, image2);
	CString str;

	switch (nRet)
	{
	case e_ImageCompareResult_OK:
		str.Format(_T("불러온 두 개의 이미지가 일치합니다."));
		//str.Format(_T("불러온 두 개의 이미지 [%s], [%s] 가 일치합니다."), strFile1, strFile2);
		AddLog(str);
		break;

	case e_ImageCompareResult_ImageEmpty_No1:
		str.Format(_T("1번의 이미지 파일이 비어 있습니다. 파일을 확인해 주십시오."));
		AddLog(str);
		break;

	case e_ImageCompareResult_ImageEmpty_No2:
		str.Format(_T("2번의 이미지 파일이 비어 있습니다. 파일을 확인해 주십시오."));
		AddLog(str);
		break;

	case e_ImageCompareResult_ImageEmpty_ALL:
		str.Format(_T("1번과 2번의 이미지 파일이 비어 있습니다. 파일을 확인해 주십시오."));
		AddLog(str);
		break;

	case e_ImageCompareResult_SizeError:
		str.Format(_T("불러온 두 개의 이미지의 크기가 서로 다릅니다."));
		AddLog(str);
		break;

	case e_ImageCompareResult_NG:
		str.Format(_T("불러온 두 개의 이미지가 일치하지 않습니다."));
		//str.Format(_T("불러온 두 개의 이미지 [%s], [%s] 가 일치하지 않습니다."), strFile1, strFile2);
		AddLog(str);
		break;

	default:
		break;
	}
}

int CCustomBlurDlg::CheckImageCompare(cv::Mat img1, cv::Mat img2)
{
	int nRet = e_ImageCompareResult_OK;

	// 예외처리 (둘 다 empty면 3을 반환)
	nRet |= (img1.empty() ? e_ImageCompareResult_ImageEmpty_No1 : 0);
	nRet |= (img2.empty() ? e_ImageCompareResult_ImageEmpty_No2 : 0);

	if (nRet != e_ImageCompareResult_OK)
		return nRet;

	// 이미지 width, height
	if ((img1.cols == img2.cols && img1.rows == img2.rows) == FALSE)
		return e_ImageCompareResult_SizeError;

	// 비교 검사
	cv::Mat dstimg;
	cv::absdiff(img1, img2, dstimg);

	BOOL bSameFlag = TRUE;
	int nColor, nCount = 0;
	for (int i = 0; i < dstimg.cols; i++)
	{
		for (int j = 0; j < dstimg.rows; j++)
		{
			//if (bSameFlag == FALSE)
			//	break;

			nColor = dstimg.at<uchar>(j, i);
			if (nColor != 0)
			{
				nCount++;
				bSameFlag &= FALSE;
				nRet = e_ImageCompareResult_NG;
				//break;
			}
		}
	}

	CString str;
	str.Format(_T("[다른 픽셀 수] : %d개"), nCount);
	AddLog(str);

	return nRet;
}

cv::Mat CCustomBlurDlg::ProcessingBlur_OpenCV(cv::Mat srcMat, int nMinKernelSize, int nMaxKernelSize)
{
	cv::Mat dstMat;

	if (srcMat.empty())
	{
		AddLog(_T("처리할 이미지가 없습니다. 이미지 버퍼를 확인하세요."));
		return dstMat;
	}

	cv::Size kernelSize(nMinKernelSize, nMaxKernelSize);
	cv::blur(srcMat, dstMat, kernelSize, cv::Point(-1, -1), cv::BORDER_DEFAULT);

	return dstMat;
}

LONGLONG CCustomBlurDlg::StartTimer()
{
	LARGE_INTEGER nCounter;
	QueryPerformanceCounter(&nCounter);
	return nCounter.QuadPart;
}

int64_t CCustomBlurDlg::EndTimer(LONGLONG startTime)
{
	LARGE_INTEGER nCounter;
	LARGE_INTEGER nFrequency;

	QueryPerformanceCounter(&nCounter);
	QueryPerformanceFrequency(&nFrequency);

	int64_t result = ((nCounter.QuadPart - startTime) / (nFrequency.QuadPart / 1000));
	return result;
}

void CCustomBlurDlg::OnBnClickedCancel()
{
	// <!-- Main Running Thread -->
	if (m_hMainRunningThread)
	{
		WaitForMultipleObjects(m_nThreads, m_hMainRunningThread, TRUE, INFINITE);

		// 스레드 핸들 배열에 해제
		for (unsigned int iThread = 0; iThread < m_nMaxThreads; iThread++)
		{
			if (m_hMainRunningThread[iThread] != NULL)
				CloseHandle(m_hMainRunningThread[iThread]);
		}

		delete[] m_hMainRunningThread;
		m_hMainRunningThread = NULL;
	}

	if (m_hThreadEndEvents)
	{
		delete[] m_hThreadEndEvents;
		m_hThreadEndEvents = NULL;
	}

	// <!-- Wait End Running Thread -->
	if (m_hManageThread)
	{
		WaitForSingleObject(m_hManageThread, INFINITE);
		CloseHandle(m_hManageThread);

		// delete m_hManageThread;
		// m_hManageThread = NULL;
	}

	CDialogEx::OnCancel();
}

void CCustomBlurDlg::SetDataToReport(BOOL bWriteFlag, int nThreadNum, CString strFileName, int nWidth, int nHeight, DWORD dwMemoryUsage, LONGLONG lfTactTime)
{
	m_reportData.bWriteFlag[nThreadNum] = TRUE;
	m_reportData.No[nThreadNum] = nThreadNum + 1;
	m_reportData.strFileName[nThreadNum] = strFileName;
	m_reportData.nWidth[nThreadNum] = nWidth;
	m_reportData.nHeight[nThreadNum] = nHeight;
	m_reportData.lfTactTime[nThreadNum] = lfTactTime;
	m_reportData.dwUsingMemory[nThreadNum] = dwMemoryUsage;
}

void CCustomBlurDlg::WriteReport()
{
	SYSTEMTIME cur_time;
	GetLocalTime(&cur_time);

	CString str;
	CString strPathName = GetDirectory();
	CString strReportFolder;
	strReportFolder.Format(_T("%s\\report"), strPathName);

	if (!PathFileExists(strReportFolder))
	{
		if (!CreateFolder(strReportFolder))
		{
			str.Format(_T("Report 폴더 생성에 실패하였습니다."));
			AddLog(str, e_LogType_Error);
			return;
		}
	}

	CString strFIleName;
	strFIleName.Format(_T("%04d %02d %02d %02d-%02d-%02d Report.csv"), cur_time.wYear, cur_time.wMonth, cur_time.wDay, cur_time.wHour, cur_time.wMinute, cur_time.wSecond);

	CString strFilePath;
	strFilePath.Format(_T("%s\\%s"), strReportFolder, strFIleName);

	CStdioFile file;
	if (!file.Open(strFilePath, CFile::modeCreate | CFile::modeWrite | CFile::typeText))
	{
		str.Format(_T("보고서 파일을 열 수 없습니다."));
		AddLog(str, e_LogType_Error);
		return;
	}

	str.Format(_T("Report Date,%04d-%02d-%02d,%02d:%02d:%02d,,\n"), cur_time.wYear, cur_time.wMonth, cur_time.wDay, cur_time.wHour, cur_time.wMinute, cur_time.wSecond);
	file.WriteString(str);

	str.Format(_T("Start Time,%04d-%02d-%02d,%02d:%02d:%02d,,\n"),
		m_reportData.start_time.wYear, m_reportData.start_time.wMonth, m_reportData.start_time.wDay,
		m_reportData.start_time.wHour, m_reportData.start_time.wMinute, m_reportData.start_time.wSecond);
	file.WriteString(str);

	str.Format(_T("End Time,%04d-%02d-%02d,%02d:%02d:%02d,,\n"),
		m_reportData.end_time.wYear, m_reportData.end_time.wMonth, m_reportData.end_time.wDay,
		m_reportData.end_time.wHour, m_reportData.end_time.wMinute, m_reportData.end_time.wSecond);
	file.WriteString(str);

	SYSTEMTIME processingTime = GetTimeSpanAsSystemTime(m_reportData.start_time, m_reportData.end_time);
	str.Format(_T("Total Duration Time,,%02d:%02d:%02d,,,\n"), processingTime.wHour, processingTime.wMinute, processingTime.wSecond);
	file.WriteString(str);

	file.WriteString(_T(",,,,\n"));
	file.WriteString(_T(",,,,\n"));

	double lfTactTime_OpenCV = GetAvgTactTime(m_reportData, e_ProcessingType_OpenCV);
	str.Format(_T("openCV dll Avg. Tact time (ms),%.2lf,,,\n"), lfTactTime_OpenCV);
	file.WriteString(str);

	double lfTactTime_Custom = GetAvgTactTime(m_reportData, e_ProcessingType_Custom);
	str.Format(_T("custom dll Avg. Tact time (ms),%.2lf,,,\n"), lfTactTime_Custom);
	file.WriteString(str);

	file.WriteString(_T(",,,,\n"));
	//file.WriteString(_T(",,Before Running,After Running,\n"));
	file.WriteString(_T(",Avg. Running,\n"));

	double lfMemoryUsage_OpenCV = GetAvgMemoryUsage(m_reportData, e_ProcessingType_OpenCV);
	str.Format(_T("openCV dll Memory usage (MB),%.0lf,\n"), lfMemoryUsage_OpenCV);
	file.WriteString(str);

	double lfMemoryUsage_Custom = GetAvgMemoryUsage(m_reportData, e_ProcessingType_Custom);
	str.Format(_T("custom dll Memory usage (MB),%.0lf,\n"), lfMemoryUsage_Custom);
	file.WriteString(str);

	file.WriteString(_T(",,,,\n"));
	file.WriteString(_T("openCV DLL Report,,,,\n"));
	file.WriteString(_T("No.,File Name,Width (px),Height (px),Tact time (ms)\n"));

	for (int i = 0; i < MAX_THREAD; i++)
	{
		int nTemp = i + (MAX_THREAD * e_ProcessingType_OpenCV);
		if (m_reportData.bWriteFlag[nTemp] == FALSE)
			continue;

		str = GetIndividualData(i, m_reportData, e_ProcessingType_OpenCV);
		file.WriteString(str);
	}

	file.WriteString(_T(",,,,\n"));
	file.WriteString(_T("Custom DLL Report,,,,\n"));
	file.WriteString(_T("No.,File Name,Width (px),Height (px),Tact time (ms)\n"));

	for (int i = 0; i < MAX_THREAD; i++)
	{
		int nTemp = i + (MAX_THREAD * e_ProcessingType_Custom);
		if (m_reportData.bWriteFlag[nTemp] == FALSE)
			continue;

		str = GetIndividualData(i, m_reportData, e_ProcessingType_Custom);
		file.WriteString(str);
	}

	file.Close();

	CString msg;
	msg.Format(_T("보고서가 [%s]에 저장되었습니다."), strFilePath);
	AddLog(msg);
}

void CCustomBlurDlg::SetCurTime(int nType)
{
	switch (nType)
	{
	case e_TimeType_StartTime:		GetLocalTime(&m_reportData.start_time);		break;
	case e_TimeType_EndTime:		GetLocalTime(&m_reportData.end_time);		break;

	default:
		break;
	}
}

SYSTEMTIME CCustomBlurDlg::GetTimeSpanAsSystemTime(const SYSTEMTIME& stStart, const SYSTEMTIME& stEnd)
{
	// SYSTEMTIME을 COleDateTime으로 변환
	COleDateTime dtStart(stStart);
	COleDateTime dtEnd(stEnd);

	COleDateTimeSpan span = dtEnd - dtStart;
	double totalSeconds = span.GetTotalSeconds();

	int days = static_cast<int>(totalSeconds / 86400);
	int remainingSec = static_cast<int>(totalSeconds) % 86400;
	int hours = remainingSec / 3600;
	int minutes = (remainingSec % 3600) / 60;
	int seconds = remainingSec % 60;
	int milliseconds = static_cast<int>((totalSeconds - static_cast<int>(totalSeconds)) * 1000);

	SYSTEMTIME stSpan = { 0 };
	stSpan.wDay = static_cast<WORD>(days);
	stSpan.wHour = static_cast<WORD>(hours);
	stSpan.wMinute = static_cast<WORD>(minutes);
	stSpan.wSecond = static_cast<WORD>(seconds);
	stSpan.wMilliseconds = static_cast<WORD>(milliseconds);
	return stSpan;
}

double CCustomBlurDlg::GetAvgTactTime(CReportData data, int nProcessingType)
{
	int nNum, nCount = 0;
	double lfSumTactTime = 0.0;

	for (int i = 0; i < MAX_THREAD; i++)
	{
		nNum = i + (MAX_THREAD * nProcessingType);

		if (data.bWriteFlag[nNum] == TRUE)
		{
			lfSumTactTime += data.lfTactTime[nNum];
			nCount++;
		}
	}

	double lfAvgTactTime = lfSumTactTime / nCount;

	if (nCount == 0)
		return -1;

	return lfAvgTactTime;
}

double CCustomBlurDlg::GetAvgMemoryUsage(CReportData data, int nProcessingType)
{
	int nNum, nCount = 0;
	double lfSumMemoryUsage = 0.0;

	for (int i = 0; i < MAX_THREAD; i++)
	{
		nNum = i + (MAX_THREAD * nProcessingType);

		if (data.bWriteFlag[nNum] == TRUE)
		{
			lfSumMemoryUsage += (double)data.dwUsingMemory[nNum];
			nCount++;
		}
	}

	double lfAvgMemoryUsage = lfSumMemoryUsage / nCount;

	if (nCount == 0)
		return -1;

	return lfAvgMemoryUsage;
}

CString CCustomBlurDlg::GetIndividualData(int nNum, CReportData data, int nProcessingType)
{
	CString str;
	nNum += (MAX_THREAD * nProcessingType);

	int nTempNum = data.No[nNum] - (MAX_THREAD * nProcessingType);		// 프로세싱 타입에 따른 No. 를 보정

	str.Format(_T("%d,%s,%d,%d,%I64d\n"), nTempNum, data.strFileName[nNum], data.nWidth[nNum], data.nHeight[nNum], data.lfTactTime[nNum]);

	if (data.bWriteFlag[nNum] == FALSE)
		return _T("");

	return str;
}

void CCustomBlurDlg::DialogLockORUnlock(BOOL bFlag)
{
	GetDlgItem(IDC_EDIT_KERNELSIZE)->EnableWindow(bFlag);
	GetDlgItem(IDC_BUTTON_IMAGELOAD)->EnableWindow(bFlag);
	GetDlgItem(IDC_BUTTON_IMAGEDELETE)->EnableWindow(bFlag);

	GetDlgItem(IDC_BUTTON_DEMORUN)->EnableWindow(bFlag);
	GetDlgItem(IDC_BUTTON_RUN)->EnableWindow(bFlag);
	GetDlgItem(IDC_BUTTON_RUN2)->EnableWindow(bFlag);
	GetDlgItem(IDC_BUTTON_MANUAL)->EnableWindow(bFlag);
	GetDlgItem(IDC_BUTTON_MANUAL2)->EnableWindow(bFlag);

	GetDlgItem(IDC_BUTTON_GRAY)->EnableWindow(bFlag);
	GetDlgItem(IDC_BUTTON_COMPARE)->EnableWindow(bFlag);

	GetDlgItem(IDCANCEL)->EnableWindow(bFlag);
}
