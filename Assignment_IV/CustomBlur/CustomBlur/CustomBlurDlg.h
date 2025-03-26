
// CustomBlurDlg.h: 헤더 파일
//

#pragma once

#include "LogManager.h"
#include ".\Include\customBlurDLL\CustomBlurDLL.h"
#include ".\Include\customBlurDLL\OpenCVBlurDLL.h"
//#pragma comment (lib, ".\\Include\\Custom\\CustomBlurDLL.lib")

// opencv
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>

enum ProcessingType : int
{
	e_ProcessingType_UNKNOWN = -1,
	e_ProcessingType_OpenCV,
	e_ProcessingType_Custom,

	e_ProcessingType_MAX
};

enum FileType : int
{
	e_File_UNKNOWN = -1,
	e_File_Init,									// Init 파일 쓰기
	e_File_Log,										// Log 쓰기 (2025. 03. 25. Thread-safe logger 작성 이후에는 초기화 외에는 쓰지 않음 -> 정리 필요)

	e_File_MAX
};

enum DLLLoadFlag : int
{
	e_DLLLoadFlag_UNKNOWN = -1,
	e_DLLLoadFlag_OK,								// DLL 로드 정상
	e_DLLLoadFlag_CustomDLL_LoadError,				// Custom DLL 로드 에러
	e_DLLLoadFlag_OpenCVWarpDLL_LoadError,			// OpenCV wrap DLL 로드 에러
	e_DLLLoadFlag_ALL_LoadError,					// 둘다 로드 못함

	e_DLLType_MAX
};

enum LogType : int
{
	e_LogType_UNKNOWN = -1,
	e_LogType_NotWrite,								// 로그 파일에 기록하지 않습니다
	e_LogType_Information,							// [IMFORMATION]
	e_LogType_Complete,								// [COMPLETE]
	e_LogType_Caution,								// [CAUTION]
	e_LogType_Error,								// [ERROR]

	e_LogType_MAX
};

enum ImageCompareResult : int
{
	e_ImageCompareResult_UNKNOWN = -1,
	e_ImageCompareResult_OK,						// 2개 비교한 이미지가 동일함
	e_ImageCompareResult_ImageEmpty_No1 = 0x01,		// 첫번째 이미지가 없습니다
	e_ImageCompareResult_ImageEmpty_No2 = 0x02,		// 두번째 이미지가 없습니다
	e_ImageCompareResult_ImageEmpty_ALL = 0x03,		// 두개의 이미지가 없습니다
	e_ImageCompareResult_SizeError,					// 두개의 이미지의 픽셀 사이즈가 다릅니다
	e_ImageCompareResult_NG,						// 2개 비교한 이미지가 다름 (사이즈, Pixel 값 등 어느 것이라도 1개가 다르면 이것을 반환함)

	e_ImageCompareResult_MAX
};

enum TimeType : int
{
	e_TimeType_UNKNOWN = -1,
	e_TimeType_StartTime,
	e_TimeType_EndTime,

	e_TimeType_MAX
};

struct CProgramOption
{
	BOOL bInitFlag = FALSE;										// .ini 파일을 통해서 정상적으로 init 했는지 여부

	int nKernelSize = 21;										// 커널 사이즈
	CString strImageFileDir = _T(".\\images\\");				// 이미지를 불러오는 폴더 경로
	CString strSaveImageFileDir = _T(".\\saveimages\\");		// 이미지를 저장하는 폴더 경로
};

struct CReportData
{
	void Reset()
	{
		start_time = { 0 };
		end_time = { 0 };

		for (int i = 0; i < MAX_THREAD * 2; i++)
		{
			bWriteFlag[i] = FALSE;
			No[i] = 0;
			strFileName[i] = _T("");
			nWidth[i] = 0;
			nHeight[i] = 0;
			dwUsingMemory[i] = 0;
			lfTactTime[i] = 0;
		}
	};

	SYSTEMTIME	start_time;
	SYSTEMTIME	end_time;

	BOOL		bWriteFlag[MAX_THREAD * 2];					// 해당 배열에 데이터가 있는지? 없으면 쓰지 말아야함
	int			No[MAX_THREAD * 2];							// 순번
	CString		strFileName[MAX_THREAD * 2];				// 파일명
	int			nWidth[MAX_THREAD * 2];
	int			nHeight[MAX_THREAD * 2];
	DWORD		dwUsingMemory[MAX_THREAD * 2];				// 사용한 메모리 점유율
	LONGLONG	lfTactTime[MAX_THREAD * 2];					// Tact Time
};

// CCustomBlurDlg 대화 상자
class CCustomBlurDlg : public CDialogEx
{
	// 생성입니다.
public:
	CCustomBlurDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.
	CLogManager m_logManager;

	// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CUSTOMBLUR_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.

	// 구현입니다.	

private:
	typedef BOOL(*CustomImageBlur)(const ImageObject*, ImageObject*, const int);

	BOOL m_bInitFlag = TRUE;						// FALSE 면 프로그램이 켜질수없다 (Init 함수 외에는 쓰지않음)
	CProgramOption m_option;						// 설정 데이터 (파일 경로, 커널 사이즈 등)
	CReportData m_reportData;

	int m_nProcessingType = e_ProcessingType_UNKNOWN;

	CListBox m_list_log;
	CListBox m_list_originImage;

	cv::Mat m_srcMat[MAX_THREAD];			// opencvBuffer (전처리 전)
	cv::Mat m_dstMat[MAX_THREAD];			// opencvBuffer (전처리 후)

	HMODULE	m_hModule_CustomDLL = NULL;		// Custom Blur dll에 대한 모듈 데이터	(dll 불러오기, dll 내 함수 실행 시 필요)
	HMODULE	m_hModule_OpenCVDLL = NULL;		// OpenCV dll에 대한 모듈 데이터		(dll 불러오기, dll 내 함수 실행 시 필요)

protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

public:
	SYSTEM_INFO m_info;

	// MainRunningThread Param
	BOOL m_bAutoRunFlag[MAX_THREAD] = { FALSE, };			// 현재 검사중인 스레드 확인 여부
	const unsigned int m_nThreads = MAX_THREAD;
	HANDLE* m_hMainRunningThread = NULL;
	HANDLE* m_hThreadEndEvents = new HANDLE[m_nThreads];
	int m_nMaxThreads = 0;									// 실제 사용한 스레드의 최대 개수 (프로그램 종료할 때 안 썼던 스레드 해제 방지용)

	// ManageThread Param
	HANDLE m_hManageThread = NULL;

	int getOriginImageCount() { return m_list_originImage.GetCount(); }

	afx_msg void OnBnClickedButtonImageload();
	afx_msg void OnBnClickedButtonImagedelete();
	afx_msg void OnBnClickedButtonDemorun();
	afx_msg void OnBnClickedButtonRun();
	afx_msg void OnBnClickedButtonRun2();
	afx_msg void OnBnClickedButtonManualRun();   		// OpenCV DLL Run
	afx_msg void OnBnClickedButtonManualRun2();			// Custom DLL Run
	afx_msg void OnBnClickedButtonGray();
	afx_msg void OnBnClickedButtonCompare();
	afx_msg void OnBnClickedCancel();

	void BeginThread(BOOL bDEMOMODE = FALSE);
	void ReadImage(CString strFileName);
	BOOL RUNOpenCVDLL(int nNum);
	BOOL RUNCustomDLL(int nNum);
	BOOL CopyMatToImageObject(const cv::Mat& mat, ImageObject* obj);			// OpenCV Mat -> Custom Buffer
	BOOL CopyImageObjectToMat(const ImageObject* obj, cv::Mat& mat);			// Custom Buffer -> OpenCV Mat
	BOOL VerifyImageIsGrayscale(CString strFileName);
	cv::Mat ProcessingBlur_OpenCV(cv::Mat srcMat, int nMinKernelSize, int nMaxKernelSize);
	BOOL CheckImageCompare(cv::Mat img1, cv::Mat img2);
	int GetImageCount() { return m_list_originImage.GetCount(); }
	int GetProcessingType() { return m_nProcessingType; }
	void SetProcessingType(int nType) { m_nProcessingType = nType; }
	void DialogLockORUnlock(BOOL bFlag);

	BOOL WriteFile(CString str, const int nCase = e_File_UNKNOWN);
	BOOL CreateFolder(const CString& strFolderPath);
	CProgramOption LoadOptionFile();
	CString GetDirectory();
	void SplitFilePath(const CString& fullPath, CString& directory, CString& fileName, CString& extension);

	void InitDLL();
	void Init();
	void AddLog(CString strLog, int nLogType = e_LogType_NotWrite);
	void AddOriginalImage(CString strPath);
	
	// Tact Time 관련 함수
	LONGLONG StartTimer();
	int64_t EndTimer(LONGLONG startTime);

	void ResetAutorunFlag()
	{
		for (int i = 0; i < MAX_THREAD; i++)
			m_bAutoRunFlag[i] = FALSE;
	}

	// Report 관련 함수
	void SetDataToReport(BOOL bWriteFlag, int nThreadNum, CString strFileName, int nWidth, int nHeight, DWORD dwMemoryUsage, LONGLONG lfTactTime);
	void ResetReportData() { m_reportData.Reset(); }
	CReportData GetDataToReport(int nThreadNum) { return m_reportData; }
	void CCustomBlurDlg::WriteReport();
	void SetCurTime(int nType);
	SYSTEMTIME GetTimeSpanAsSystemTime(const SYSTEMTIME& stStart, const SYSTEMTIME& stEnd);
	double GetAvgTactTime(CReportData data, int nProcessingType);
	double GetAvgMemoryUsage(CReportData data, int nProcessingType);
	CString GetIndividualData(int nNum, CReportData data, int nProcessingType);	
};

// 프로그램 실행 시 항상 돌아가고 있는 쓰레드에 대한 구조체
struct ManageThreadParam
{
	CCustomBlurDlg* pDlg;		// 메인 다이얼로그
	BOOL bDEMOMode = FALSE;		// 데모모드
	int nMaxSequence = 0;		// 스레드를 몇번 돌릴지 결정 (단일 : 1, 데모 : 2... 추가로 확장 가능?)
};

// blur 수행하는 메인 쓰레드에 대한 구조체
struct MainRunningThreadParam
{
	CCustomBlurDlg* pDlg;							// 메인 다이얼로그
	int nThreadNum = 0;								// 스레드 번호
	int nProcessingType = e_ProcessingType_UNKNOWN;	// 프로세싱 타입 (= enum ProcessingType : int)
	HANDLE hThreadEndEvent;							// 스레드 끝났는지 확인하는 이벤트
	// ... 기타 필요한 다른 변수 추가
};
