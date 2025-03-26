#pragma once
#include <afx.h>
#include <list>
#include <windows.h>
#include <tchar.h>

/*
    [Log Concept]
    1. MainDlg에서 각 부분에 있는 AddLog 함수를 통해 m_logList 에 로그가 순차적으로 담김
    2. 주기적 (m_nLogWriteTime) 으로 m_logList의 데이터는 FlushLogs 내 변수와 맞바뀜
        (즉, m_logList는 다시 초기화되며 FlushLogs 내 지역 변수에 해당 로그가 담김)
    3. FlushLogs 에서 파일에다가 로그를 쓴다
*/

class CLogManager
{
public:
    CLogManager();
    ~CLogManager();

    void AddLog(const CString& str);            // Dlg 에서 로그를 쓰는 함수

    BOOL Start();
    void Stop();

private:
    const int m_nLogWriteTime = 1000;           // 로그 작성 주기 (1000 = 1초)

    void FlushLogs();                           // 로그 객체 -> 파일

    CRITICAL_SECTION m_cs;
    std::list<CString> m_logList;               // 로그 저장 리스트

    static DWORD WINAPI LogThreadProc(LPVOID pParam);
    HANDLE m_hLogThread;
    BOOL m_bRunning;                            // 스레드 실행 Flag
};