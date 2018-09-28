// Proj2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <winsock2.h>

const long JAN_1ST_1900 = 2415021;					//Number of days since 1st january 4712 BC, to 1st junuary 1900 AD
const double NTP_FRACTIONAL_TO_MS = (((double)1000.0)/0xFFFFFFFF);	//Divide the number of miliseconds in 1 second by 2^32

typedef struct
{
	DWORD dwSecond;			//Number of seconds
	DWORD dwFract;			//Number of miliseconds
} NtpTimePacket;			//Structure, that will send to server
											//and data from server will be receive in structure
//
// Calculate Gregorian Date from Julian Day
//
void GetGregorianDate(long JD, WORD& Year, WORD& Month, WORD& Day)
{   
	long j = JD - 1721119;        //Number of days from 4712 B.C., 1721119 - number of days since 0 year by the Julian calendar(1 junuary 4712 BC) to 1st March 1 year AD
																//Вычитая это значение, мы получаем сколько дней прошло с марта 1 года. 

	long y = (4 * j - 1) / 146097;//Number of century, 146097 - number of days in 400 years of Gregorian calendar, including leap year
	j = 4 * j - 1 - 146097 * y;   //Nuber of days*4 in this century
	long d = j / 4;               //Number of days in this century
	j = (4 * d + 3) / 1461;       //Nuber of years in this century, 1461 - number of days in 4 years of Gregorian calendar, including leap year
	d = 4 * d + 3 - 1461 * j;
	d = (d + 4) / 4;
	long m = (5 * d - 3) / 153;		//153 - Number of days in 5 mounth, 31 day in each
	d = 5 * d - 3 - 153 * m;
	d = (d + 5) / 5;              //Date of today
	y = 100 * y + j;
	if (m < 10)										//Add 3 or subtract 9 to return the start of year in 1 junuary
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
	return (WORD)((((double)dwFraction) * NTP_FRACTIONAL_TO_MS) + 0.5);	//convert number of miliseconds from NTP-format
}																																			//in a convenient for System Time view

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
	int iErr = WSAStartup(MAKEWORD(2,2), &wsaData);	//Initialize the use of the Winsock DLL 
	if (iErr != 0 )
	{
		printf("Error WSAStartup: %d\r\n", iErr);
		return 1;
	}

	char szClientBuf[48]={0};								//Client buffer
	char szServerBuf[48]={0};								//Server buffer, which will be written, after message will be accepted

	{ // write first byte
		int iDeltaLen = 0;                   // current position of write
		szClientBuf[0] |= 0x3;               // Leap Indicator (2 bits). 3 - Unknown (clock unsynchronized)
		iDeltaLen += 2;                      // increase position by field length.
		szClientBuf[0] |= 0x4 << iDeltaLen;  // Version Number — current version 4. (3 bits).
		iDeltaLen += 3;                      // increase position by field length.
		szClientBuf[0] |= 0x3 << iDeltaLen;  // Mode(3 bit). 3 - client.
	}

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);	//create socket
	if (sock == INVALID_SOCKET)
	{
		printf("Error socket: %d\r\n", WSAGetLastError());
		return 1;
	}

	struct sockaddr_in dest_addr;
	dest_addr.sin_family = AF_INET;															//Protocol family: Internet
	dest_addr.sin_port   = htons(123);													//Setup IP port
	dest_addr.sin_addr.s_addr=inet_addr("158.43.128.33");				//Adress to which the message will be send
	//dest_addr.sin_addr.s_addr=inet_addr("132.163.96.1");
	// time.windows.com: 
	// time.nist.gov: 132.163.96.1
	// time-nw.nist.gov: 

	FILETIME ftmClSend, ftmClRecv;				//Time of send and recieve of message
	NtpTimePacket ntpTm_Receive;					//Time of recieve to server
	NtpTimePacket ntpTm_Transmit;					//Time of send to client

	::GetSystemTimeAsFileTime(&ftmClSend); // time of send to server

	int iRes = 0;
	iRes = sendto(sock, szClientBuf, sizeof(szClientBuf), 0, (sockaddr *) &dest_addr, sizeof(dest_addr));	//Send message on server
	if (SOCKET_ERROR == iRes)
	{
		printf("Error sendto: %d\r\n", WSAGetLastError());
		return 1;
	}

	sockaddr_in serv_addr;
	int iServAddrSize = sizeof(serv_addr);

	iRes = recvfrom(sock, szServerBuf, sizeof(szServerBuf), 0, (sockaddr *) &serv_addr, &iServAddrSize);	//Recieve message form server
	if (SOCKET_ERROR == iRes)
	{
		printf("Error recvfrom: %d\r\n", WSAGetLastError());
		return 1;
	}

	::GetSystemTimeAsFileTime(&ftmClRecv); // time of recieve from server to client

	ntpTm_Receive.dwSecond   = htonl(*((DWORD*)(szServerBuf + 32)));//htonl - convert bytes form TCP/IP
	ntpTm_Receive.dwFract    = htonl(*((DWORD*)(szServerBuf + 36)));// in Big-endian

	ntpTm_Transmit.dwSecond  = htonl(*((DWORD*)(szServerBuf + 40)));
	ntpTm_Transmit.dwFract   = htonl(*((DWORD*)(szServerBuf + 44)));

	SYSTEMTIME stmSend, stmRecv;
	::FileTimeToSystemTime(&ftmClSend, &stmSend);	//Convert time of send to server/recieve form server
	::FileTimeToSystemTime(&ftmClRecv, &stmRecv);	// to system time


	SYSTEMTIME stIn;
	ConvNtpToSystemTime(ntpTm_Receive, stIn);
	SYSTEMTIME stOut;
	ConvNtpToSystemTime(ntpTm_Transmit, stOut);

	FILETIME ftmServIn, ftmServOut;
	::SystemTimeToFileTime(&stIn,  &ftmServIn);	//Convert time of recieve to server/send to client
	::SystemTimeToFileTime(&stOut, &ftmServOut);//in system time

	#define PrntT(prefix, ftm, t) { printf("%s:\t   (ftm (UTC): %08u.%08u), %I64u\r\n", prefix, ftm.dwHighDateTime, ftm.dwLowDateTime, t); }
	#define MAKEUINT64(lo,hi) ((UINT64)(DWORD)(lo) | (UINT64)(DWORD)(hi) << 32)

	INT64 t0 = MAKEUINT64(ftmClSend.dwLowDateTime,  ftmClSend.dwHighDateTime);	//Write the number of miliseconds and seconds into one variable
	INT64 t1 = MAKEUINT64(ftmServIn.dwLowDateTime,  ftmServIn.dwHighDateTime);
	INT64 t2 = MAKEUINT64(ftmServOut.dwLowDateTime, ftmServOut.dwHighDateTime);
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

	// convert to miliseconds
	dRoundTripDelay /= 10000;
	dLocalClockOffset /= 10000;

	#define PrntLocTm(prefix, stm, ftm) { \
		printf("%s:\t   %04d.%02d.%02d %02d:%02d:%02d.%03d      (ftm (UTC): %08u.%08u)\r\n", prefix, \
		stm.wYear, stm.wMonth, stm.wDay, stm.wHour, stm.wMinute, stm.wSecond, stm.wMilliseconds, \
		ftm.dwHighDateTime, ftm.dwLowDateTime); }

	// delta, which must add to time in computer
	printf("\r\nTime offset: %s %f ms \r\n", dLocalClockOffset < 0 ? "" : "+", dLocalClockOffset);
	//Round Trip Delay between server and client
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
