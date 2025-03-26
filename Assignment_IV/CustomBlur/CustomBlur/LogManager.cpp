#include "pch.h"
#include "LogManager.h"

CLogManager::CLogManager() : m_hLogThread(NULL), m_bRunning(FALSE)
{
    InitializeCriticalSection(&m_cs);
}

CLogManager::~CLogManager()
{
    Stop();
    DeleteCriticalSection(&m_cs);
}

void CLogManager::AddLog(const CString& str)
{
    EnterCriticalSection(&m_cs);            // 동시에 로그 중첩되는것을 방지
    m_logList.push_back(str);
    LeaveCriticalSection(&m_cs);
}

BOOL CLogManager::Start()
{
    m_bRunning = TRUE;
    m_hLogThread = CreateThread(NULL, 0, LogThreadProc, this, 0, NULL);
    if (m_hLogThread == NULL)
    {
        m_bRunning = FALSE;
        return FALSE;
    }
    return TRUE;
}

void CLogManager::Stop()
{
    m_bRunning = FALSE;
    if (m_hLogThread)
    {
        // 스레드가 종료될 때까지 대기
        WaitForSingleObject(m_hLogThread, INFINITE);
        CloseHandle(m_hLogThread);
        m_hLogThread = NULL;
    }
}

// 일정 주기마다 FlushLogs 호출
DWORD WINAPI CLogManager::LogThreadProc(LPVOID pParam)
{
    CLogManager* pLogMgr = reinterpret_cast<CLogManager*>(pParam);
    while (pLogMgr->m_bRunning)
    {
        Sleep(pLogMgr->m_nLogWriteTime);  // 1초마다 로그 flush (변수에서 설정하세요)
        pLogMgr->FlushLogs();
    }

    // 스레드가 종료될 때 남은 로그 flush
    pLogMgr->FlushLogs();
    return 0;
}

// 주기적으로 로그를 파일로 기록하는 함수
void CLogManager::FlushLogs()
{
    // 멤버변수에 있는 로그들을 tempList와 맞바꿔치기
    std::list<CString> tempList;
    EnterCriticalSection(&m_cs);
    tempList.swap(m_logList);
    LeaveCriticalSection(&m_cs);

    if (tempList.empty())
        return;

    // 로그 작성
    CString str;
    CString strFileName = LOG_FILE;
    CStdioFile file;

    BOOL bRet = TRUE;
    BOOL bFileFindFlag = FALSE;
    CFileFind ff;

    if (ff.FindFile((LPCTSTR)strFileName))		bFileFindFlag = TRUE;
    else										bFileFindFlag = FALSE;

    str.Append(_T("\n"));
    if (bFileFindFlag)	bRet &= file.Open(strFileName, CFile::modeWrite | CFile::typeText);
    else				bRet &= file.Open(strFileName, CFile::modeCreate | CFile::modeWrite | CFile::typeText);

    if (!bRet)
    {
        AfxMessageBox(_T("Log File 열기에 실패하였습니다."));
        return;
    }

    file.SeekToEnd();

    // tempList에 있는 모든 로그를 파일에다 작성
    for (const auto& logMsg : tempList)
        file.WriteString(logMsg + _T("\n"));

    file.Close();
}