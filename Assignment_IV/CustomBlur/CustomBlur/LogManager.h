#pragma once
#include <afx.h>
#include <list>
#include <windows.h>
#include <tchar.h>

/*
    [Log Concept]
    1. MainDlg���� �� �κп� �ִ� AddLog �Լ��� ���� m_logList �� �αװ� ���������� ���
    2. �ֱ��� (m_nLogWriteTime) ���� m_logList�� �����ʹ� FlushLogs �� ������ �¹ٲ�
        (��, m_logList�� �ٽ� �ʱ�ȭ�Ǹ� FlushLogs �� ���� ������ �ش� �αװ� ���)
    3. FlushLogs ���� ���Ͽ��ٰ� �α׸� ����
*/

class CLogManager
{
public:
    CLogManager();
    ~CLogManager();

    void AddLog(const CString& str);            // Dlg ���� �α׸� ���� �Լ�

    BOOL Start();
    void Stop();

private:
    const int m_nLogWriteTime = 1000;           // �α� �ۼ� �ֱ� (1000 = 1��)

    void FlushLogs();                           // �α� ��ü -> ����

    CRITICAL_SECTION m_cs;
    std::list<CString> m_logList;               // �α� ���� ����Ʈ

    static DWORD WINAPI LogThreadProc(LPVOID pParam);
    HANDLE m_hLogThread;
    BOOL m_bRunning;                            // ������ ���� Flag
};