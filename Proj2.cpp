// Proj2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <winsock2.h>

const long JAN_1ST_1900 = 2415021;					//���������� ���� � 1 ������ 4712 ���� �� ����� ��� �� 1 ������ 1900 ���� ����� ���
const double NTP_FRACTIONAL_TO_MS = (((double)1000.0)/0xFFFFFFFF);	//����� ���������� ���������� � �������, �� 2^32

typedef struct
{
	DWORD dwSecond;			//���������� ������
	DWORD dwFract;			//����������� ����������
} NtpTimePacket;			//���������, ������� ����� ������������ �� ������
											//� ���� ����� ������������ ������ ��������� � �������
//
// Calculate Gregorian Date from Julian Day
//
void GetGregorianDate(long JD, WORD& Year, WORD& Month, WORD& Day)
{   
	long j = JD - 1721119;        //Number of days from 4712 B.C., 1721119 - ���������� ���� �� 0 ���� �� ���������� ���������(1 ������ 4712 �� ����� ���) �� ����� 1 ����� 1 ���� ����� ���
																//������� ��� ��������, �� �������� ������� ���� ������ � ����� 1 ����. 

	long y = (4 * j - 1) / 146097;//Number of century, 146097 - ������� ���� � 400 ����� �������������� ���������, ��� �� ��� ����������� ���������� ���
	j = 4 * j - 1 - 146097 * y;   //Nuber of days*4 in this century
	long d = j / 4;               //Number of days in this century
	j = (4 * d + 3) / 1461;       //Nuber of years in this century, 1461 - ���������� ���� � 4 ����� �� �������������� ��������� � ������ ����������� ����
	d = 4 * d + 3 - 1461 * j;
	d = (d + 4) / 4;
	long m = (5 * d - 3) / 153;		//153 - ���������� ���� � 5 �������, � 31 ���� � ������
	d = 5 * d - 3 - 153 * m;
	d = (d + 5) / 5;              //Date of today
	y = 100 * y + j;
	if (m < 10)										//��������� 3 ��� �������� 9 ��� �� ������������ ������ ���� � 1 ������
		m = m + 3;
	else
	{
		m = m - 9;
		y = y + 1;
	}

	Year = (WORD) y;              //This year
	Month = (WORD) m;             //This month
	Day = (WORD) d;               //Today
}

WORD NtpFractionToMs(DWORD dwFraction)
{
	return (WORD)((((double)dwFraction) * NTP_FRACTIONAL_TO_MS) + 0.5);	//��������� ���������� ���������� �� NTP
}																																			// � ������� ��� System Time ���

void ConvNtpToSystemTime(NtpTimePacket& ntpTm, SYSTEMTIME& st)
{
	DWORD s = ntpTm.dwSecond;
	st.wSecond = (WORD)(s % 60);//second
	s /= 60;
	st.wMinute = (WORD)(s % 60);//minute
	s /= 60;
	st.wHour = (WORD)(s % 24);//hour
	s /= 24;//day
	long JD = s + JAN_1ST_1900;//Julian day
	st.wDayOfWeek = (WORD)((JD + 1) % 7);//day of week
	GetGregorianDate(JD, st.wYear, st.wMonth, st.wDay);//to Greorian date
	st.wMilliseconds = NtpFractionToMs(ntpTm.dwFract);//to MS
}

int _tmain(int argc, _TCHAR* argv[])
{
	WSADATA wsaData = {0};
	int iErr = WSAStartup(MAKEWORD(2,2), &wsaData);	//�������������� ������������� Winsock DLL 
	if (iErr != 0 )
	{
		printf("Error WSAStartup: %d\r\n", iErr);
		return 1;
	}

	char szClientBuf[48]={0};								//���������� �����
	char szServerBuf[48]={0};								//��������� �����, ������� ����� ����������, ����� �������� ���������

	{ // ��������� ������ ����
		int iDeltaLen = 0;                   // ������� �� �������� ��������.
		szClientBuf[0] |= 0x3;               // ��������� ��������� (2 ����). 3 - ���������� (����� �� ����������������).
		iDeltaLen += 2;                      // ����������� ������� �� ����� ����.
		szClientBuf[0] |= 0x4 << iDeltaLen;  // ����� ������ (��) � ������� �������� 4. (3 ����).
		iDeltaLen += 3;                      // ����������� ������� �� ����� ����.
		szClientBuf[0] |= 0x3 << iDeltaLen;  // �����(3 ����). 3 - ������.
	}

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);	//������� �����
	if (sock == INVALID_SOCKET)
	{
		printf("Error socket: %d\r\n", WSAGetLastError());
		return 1;
	}

	struct sockaddr_in dest_addr;
	dest_addr.sin_family = AF_INET;															//��������� ����������: ��������
	dest_addr.sin_port   = htons(123);													//������������� IP ����
	dest_addr.sin_addr.s_addr=inet_addr("158.43.128.33");				//��������� �����, ���� ���������� ���������
	//dest_addr.sin_addr.s_addr=inet_addr("132.163.96.1");
	// time.windows.com: 
	// time.nist.gov: 132.163.96.1
	// time-nw.nist.gov: 

	FILETIME ftmClSend, ftmClRecv;				//����� �������� � �������� �������� ���������
	NtpTimePacket ntpTm_Receive;					//����� �������� ��������� � �������
	NtpTimePacket ntpTm_Transmit;					//����� �������� ��������� �� �������

	::GetSystemTimeAsFileTime(&ftmClSend); // ����� �������� ������

	int iRes = 0;
	iRes = sendto(sock, szClientBuf, sizeof(szClientBuf), 0, (sockaddr *) &dest_addr, sizeof(dest_addr));	//�������� ��������� �� ������
	if (SOCKET_ERROR == iRes)
	{
		printf("Error sendto: %d\r\n", WSAGetLastError());
		return 1;
	}

	sockaddr_in serv_addr;
	int iServAddrSize = sizeof(serv_addr);

	iRes = recvfrom(sock, szServerBuf, sizeof(szServerBuf), 0, (sockaddr *) &serv_addr, &iServAddrSize);	//�������� ��������� �� �������
	if (SOCKET_ERROR == iRes)
	{
		printf("Error recvfrom: %d\r\n", WSAGetLastError());
		return 1;
	}

	::GetSystemTimeAsFileTime(&ftmClRecv); // ����� ������ ������

	ntpTm_Receive.dwSecond   = htonl(*((DWORD*)(szServerBuf + 32)));//htonl - ��������� ����� �� TCP/IP
	ntpTm_Receive.dwFract    = htonl(*((DWORD*)(szServerBuf + 36)));// � Big-endian

	ntpTm_Transmit.dwSecond  = htonl(*((DWORD*)(szServerBuf + 40)));
	ntpTm_Transmit.dwFract   = htonl(*((DWORD*)(szServerBuf + 44)));

	SYSTEMTIME stmSend, stmRecv;
	::FileTimeToSystemTime(&ftmClSend, &stmSend);	//��������� ����� �������� ��
	::FileTimeToSystemTime(&ftmClRecv, &stmRecv);	// �������/������ �� �������


	SYSTEMTIME stIn;
	ConvNtpToSystemTime(ntpTm_Receive, stIn);
	SYSTEMTIME stOut;
	ConvNtpToSystemTime(ntpTm_Transmit, stOut);

	FILETIME ftmServIn, ftmServOut;
	::SystemTimeToFileTime(&stIn,  &ftmServIn);	//��������� ����� �������� �������/
	::SystemTimeToFileTime(&stOut, &ftmServOut);//�������� �� �������

	#define PrntT(prefix, ftm, t) { printf("%s:\t   (ftm (UTC): %08u.%08u), %I64u\r\n", prefix, ftm.dwHighDateTime, ftm.dwLowDateTime, t); }
	#define MAKEUINT64(lo,hi) ((UINT64)(DWORD)(lo) | (UINT64)(DWORD)(hi) << 32)

	INT64 t0 = MAKEUINT64(ftmClSend.dwLowDateTime,  ftmClSend.dwHighDateTime);	//��������� ���������� 
	INT64 t1 = MAKEUINT64(ftmServIn.dwLowDateTime,  ftmServIn.dwHighDateTime);	//���������� � ������
	INT64 t2 = MAKEUINT64(ftmServOut.dwLowDateTime, ftmServOut.dwHighDateTime);	// � ���� ����������
	INT64 t3 = MAKEUINT64(ftmClRecv.dwLowDateTime,  ftmClRecv.dwHighDateTime);

	PrntT("t0", ftmClSend,  t0);
	PrntT("t1", ftmServIn,  t1);
	PrntT("t2", ftmServOut, t2);
	PrntT("t3", ftmClRecv,  t3);
	printf("\r\n");

	

	double dRoundTripDelay   = 0; //Round trip time in seconds
	double dLocalClockOffset = 0; //Local clock offset relative to the server

	//Calculate tip delay
	dRoundTripDelay = (double)((t3 - t0) - (t2 - t1));
	//calculate offset of time
	dLocalClockOffset = (double)((t1 - t0) + (t2 - t3)) / 2;

	// ��������� � ������������
	dRoundTripDelay /= 10000;
	dLocalClockOffset /= 10000;

	#define PrntLocTm(prefix, stm, ftm) { \
		printf("%s:\t   %04d.%02d.%02d %02d:%02d:%02d.%03d      (ftm (UTC): %08u.%08u)\r\n", prefix, \
		stm.wYear, stm.wMonth, stm.wDay, stm.wHour, stm.wMinute, stm.wSecond, stm.wMilliseconds, \
		ftm.dwHighDateTime, ftm.dwLowDateTime); }

	// ������, ������� ���� ��������� � �������� �������
	printf("\r\nTime offset: %s %f ms \r\n", dLocalClockOffset < 0 ? "-" : "+", dLocalClockOffset);
	//�������� �� ���� �� ������� �� �������
	printf("\r\nRound Trip Delay: %f ms \r\n", dRoundTripDelay);
	printf("\n");

	PrntLocTm("ClSend(UTC)", stmSend, ftmClSend);

	PrntLocTm("SrvIn(UTC)", stIn, ftmServIn);

	PrntLocTm("SrvOut(UTC)", stOut, ftmServOut);

	PrntLocTm("ClRecv(UTC)", stmRecv, ftmClRecv);

	WSACleanup();

	printf("\r\ntype Enter key...\r\n");
	getchar();
	return 0;
}
