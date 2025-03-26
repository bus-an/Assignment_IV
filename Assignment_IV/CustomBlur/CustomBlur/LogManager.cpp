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
    EnterCriticalSection(&m_cs);            // ���ÿ� �α� ��ø�Ǵ°��� ����
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
        // �����尡 ����� ������ ���
        WaitForSingleObject(m_hLogThread, INFINITE);
        CloseHandle(m_hLogThread);
        m_hLogThread = NULL;
    }
}

// ���� �ֱ⸶�� FlushLogs ȣ��
DWORD WINAPI CLogManager::LogThreadProc(LPVOID pParam)
{
    CLogManager* pLogMgr = reinterpret_cast<CLogManager*>(pParam);
    while (pLogMgr->m_bRunning)
    {
        Sleep(pLogMgr->m_nLogWriteTime);  // 1�ʸ��� �α� flush (�������� �����ϼ���)
        pLogMgr->FlushLogs();
    }

    // �����尡 ����� �� ���� �α� flush
    pLogMgr->FlushLogs();
    return 0;
}

// �ֱ������� �α׸� ���Ϸ� ����ϴ� �Լ�
void CLogManager::FlushLogs()
{
    // ��������� �ִ� �α׵��� tempList�� �¹ٲ�ġ��
    std::list<CString> tempList;
    EnterCriticalSection(&m_cs);
    tempList.swap(m_logList);
    LeaveCriticalSection(&m_cs);

    if (tempList.empty())
        return;

    // �α� �ۼ�
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
        AfxMessageBox(_T("Log File ���⿡ �����Ͽ����ϴ�."));
        return;
    }

    file.SeekToEnd();

    // tempList�� �ִ� ��� �α׸� ���Ͽ��� �ۼ�
    for (const auto& logMsg : tempList)
        file.WriteString(logMsg + _T("\n"));

    file.Close();
}