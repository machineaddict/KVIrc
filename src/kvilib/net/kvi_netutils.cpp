//=============================================================================
//
//   File : kvi_netutlis.cpp
//   Creation date : Sun Jun 18 2000 18:37:27 by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2000-2008 Szymon Stefanek (pragma at kvirc dot net)
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your opinion) any later version.
//
//   This program is distributed in the HOPE that it will be USEFUL,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, write to the Free Software Foundation,
//   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//=============================================================================

#define _KVI_NETUTILS_CPP_

#define NTDDI_VERSION NTDDI_WINXP

#ifndef _WIN32_WINNT
	#define _WIN32_WINNT _WIN32_WINNT_WINXP
#endif

#include "kvi_netutils.h"
#include "kvi_memmove.h"

#include <QStringList>

#if !defined(COMPILE_ON_WINDOWS) && !defined(COMPILE_ON_MINGW)
	#include <sys/time.h> // struct timeval
	#include <unistd.h>
	#include <netdb.h>
#endif

#ifdef COMPILE_ON_WINDOWS
	#include <Ws2tcpip.h>
#endif

#include <sys/types.h>
#include "kvi_qstring.h"

#ifdef COMPILE_GET_INTERFACE_ADDRESS
	#include <sys/ioctl.h>
	#include <net/if.h>
#endif //COMPILE_GET_INTERFACE_ADDRESS

#ifndef HAVE_INET_ATON


// FIXME: #warning "Your system lacks the inet_aton function,"
// FIXME: #warning "you're trying to compile this file without"
// FIXME: #warning "the kvi_sysconfig.h created by the cmake script,"
// FIXME: #warning "Using own internal implementation of inet_aton."

#include <ctype.h>

// Need own inet_aton implementation
//
// Check whether "cp" is a valid ascii representation
// of an Internet address and convert to a binary address.
// Returns 1 if the address is valid, 0 if not.
// This replaces inet_addr, the return value from which
// cannot distinguish between failure and a local broadcast address.
//
// Original code comes from the ircd source.
//

bool kvi_stringIpToBinaryIp(const char *szIp,struct in_addr *address)
{
	register unsigned long val;
	register int base, n;
	register char c;
	unsigned int parts[4];
	register unsigned int *pp = parts;
	if(!szIp)return false;
	c = *szIp;
	for(;;){
		// Collect number up to ``.''.
		// Values are specified as for C:
		// 0x=hex, 0=octal, isdigit=decimal.
		if(!isdigit(c))return false;
		val  = 0;
		base = 10;

		if(c == '0'){
			c = *++szIp;
			if((c == 'x')||(c == 'X'))base = 16, c = *++szIp;
			else base = 8;
		}

		for (;;) {
			if(isascii(c) && isdigit(c)) {
				val = (val * base) + (c - '0');
				c   = *++szIp;
			} else if (base == 16 && isascii(c) && isxdigit(c)) {
				val = (val << 4) | (c + 10 - (islower(c) ? 'a' : 'A'));
				c   = *++szIp;
			} else break;
		}

		if(c == '.'){
			// Internet format:
			//	a.b.c.d
			//	a.b.c	(with c treated as 16 bits)
			//	a.b	(with b treated as 24 bits)

			if(pp >= (parts + 3)) return false;
			*pp++ = val;
			c     = *++szIp;
		} else break;
	}

	// Check for trailing characters.
	if ((c != '\0') && (!isascii(c) || !isspace(c)))return false;

	// Concact the address according to
	// the number of parts specified.
	n = pp - parts + 1;

	switch (n) {
		case 0: return false; // initial nondigit
		case 1:	break;        // a -- 32 bits
		case 2:               // a.b -- 8.24 bits
			if(val > 0xffffff) return false;
			val |= parts[0] << 24;
			break;
		case 3:               // a.b.c -- 8.8.16 bits
			if(val > 0xffff)return false;
			val |= (parts[0] << 24) | (parts[1] << 16);
			break;
		case 4:               // a.b.c.d -- 8.8.8.8 bits
			if(val > 0xff)return false;
			val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
			break;
	}

	if(address)address->s_addr = htonl(val);
	return true;

}

#else //!HAVE_INET_ATON

bool kvi_stringIpToBinaryIp(const char *szIp,struct in_addr *address)
{
	if(!szIp)return false;
	return (inet_aton(szIp,address) != 0);
}
#endif //!HAVE_INET_ATON



#ifndef HAVE_INET_NTOA

// FIXME: #warning "Your system lacks the inet_ntoa function,"
// FIXME: #warning "you're trying to compile this file without"
// FIXME: #warning "the config.h created by the configure script,"
// FIXME: #warning "Using own internal implementation of inet_ntoa."

//
// Original code comes from the ircd source.
//


bool kvi_binaryIpToStringIp(struct in_addr in,QString &szBuffer)
{
	unsigned char *s = (unsigned char *)&in;
	int	a,b,c,d;
	a = (int)*s++;
	b = (int)*s++;
	c = (int)*s++;
	d = (int)*s;

	szBuffer.sprintf("%d.%d.%d.%d", a,b,c,d );
	return true;
}

#else //HAVE_INET_NTOA

bool kvi_binaryIpToStringIp(struct in_addr in,QString &szBuffer)
{
// FIXME: #warning "This is NOT thread safe!"

	char * ptr = inet_ntoa(in);
	if(!ptr)return false;
	szBuffer = ptr;
	return true;
}

#endif //HAVE_INET_NTOA

bool kvi_isValidStringIp(const char *szIp)
{
	struct in_addr address;
	if(!szIp)return false;
	if(!isdigit(*szIp))return false;
	return kvi_stringIpToBinaryIp(szIp,&address);
}

#ifdef COMPILE_IPV6_SUPPORT

#if defined(COMPILE_ON_WINDOWS) || defined(COMPILE_ON_MINGW)

static unsigned int scan_ip6(const char *s,char ip[16])
{
	unsigned int i;
	unsigned int len=0;
	unsigned long u;

	char suffix[16];
	unsigned int prefixlen=0;
	unsigned int suffixlen=0;

	for (i=0; i<16; i++) ip[i]=0;

	for (;;) {
		if (*s == ':') {
			len++;
			if (s[1] == ':') {    /* Found "::", skip to part 2 */
				s+=2;
				len++;
				break;
			}
			s++;
		}

		{
			char *tmp;
			u=strtoul(s,&tmp,16);
			i=tmp-s;
		}

		if (!i) return 0;
		if (prefixlen==12 && s[i]=='.') {
			/* the last 4 bytes may be written as IPv4 address */
			if (kvi_stringIpToBinaryIp(s,(struct in_addr*)(ip+12)))
				return i+len;
			else
				return 0;
		}

		ip[prefixlen++] = (u >> 8);
		ip[prefixlen++] = (u & 255);

		s += i; len += i;

		if (prefixlen==16) return len;
	}

	/* part 2, after "::" */

	for (;;) {
		if (*s == ':') {
			if (suffixlen==0) break;
			s++;
			len++;
		} else if (suffixlen!=0) break;

		{
			char *tmp;
			u=strtol(s,&tmp,16);
			i=tmp-s;
		}

		if (!i) {
			if (*s) len--;
			break;
		}

		if (suffixlen+prefixlen<=12 && s[i]=='.') {

			if (kvi_stringIpToBinaryIp(s,(struct in_addr*)(suffix+suffixlen))) {
				suffixlen+=4;
				len+=(unsigned int)strlen(s);
				break;
			} else prefixlen=12-suffixlen;        /* make end-of-loop test true */
		}

		suffix[suffixlen++] = (u >> 8);
		suffix[suffixlen++] = (u & 255);
		s += i; len += i;
		if (prefixlen+suffixlen==16) break;
	}

	for (i=0; i<suffixlen; i++)
		ip[16-suffixlen+i] = suffix[i];

	return len;
}

int inet_pton(int AF, const char *CP, void *BUF) {
	int len;
	if (AF==AF_INET) {
		if (!kvi_stringIpToBinaryIp(CP,(struct in_addr*)BUF))
		return 0;
	} else if (AF==AF_INET6) {
		if (CP[len=scan_ip6(CP,(char *)BUF)])
		return 0;
	} else {
		errno=WSAEPFNOSUPPORT;
		return -1;
	}
	return 1;
}

static const unsigned char V4mappedprefix[12]={0,0,0,0,0,0,0,0,0,0,0xff,0xff};

static char tohex(char hexdigit) {
	return hexdigit>9?hexdigit+'a'-10:hexdigit+'0';
}

static int fmt_xlong(char* s,unsigned int i)
{
	char* bak=s;
	*s=tohex((i>>12)&0xf); if (s!=bak || *s!='0') ++s;
	*s=tohex((i>>8)&0xf); if (s!=bak || *s!='0') ++s;
	*s=tohex((i>>4)&0xf); if (s!=bak || *s!='0') ++s;
	*s=tohex(i&0xf);
	return s-bak+1;
}

static unsigned int i2a(char* dest,unsigned int x)
{
	register unsigned int tmp=x;
	register unsigned int len=0;

	if (x>=100) { *dest++=tmp/100+'0'; tmp=tmp%100; ++len; }
	if (x>=10) { *dest++=tmp/10+'0'; tmp=tmp%10; ++len; }
	*dest++=tmp+'0';
	return len+1;
}

char *inet_ntoa_r(struct in_addr in,char* buf)
{
	unsigned int len;
	unsigned char *ip=(unsigned char*)&in;

	len=i2a(buf,ip[0]); buf[len]='.'; ++len;
	len+=i2a(buf+ len,ip[1]); buf[len]='.'; ++len;
	len+=i2a(buf+ len,ip[2]); buf[len]='.'; ++len;
	len+=i2a(buf+ len,ip[3]); buf[len]=0;
	return buf;
}

unsigned int fmt_ip6(char *s,const char ip[16])
{
	unsigned int len;
	unsigned int i;
	unsigned int temp;
	unsigned int compressing; // 0 not compressing , 1 compressing now , 2 already compressed once

	len = 0;
	compressing = 0;

	for(int j=0;j<16;j+=2)
	{
		if (j==12 && !memcmp(ip,V4mappedprefix,12))
		{
			inet_ntoa_r(*(struct in_addr*)(ip+12),s);
			temp=(unsigned int)strlen(s);
			return len+temp;
		}

		temp = ((unsigned long) (unsigned char) ip[j] << 8) + (unsigned long) (unsigned char) ip[j+1];

		if(temp == 0)
		{
			if(compressing == 0)
			{
				compressing=1;
				if (j==0)
				{
					*s++=':';
					++len;
				}
			}
		} else {
			if(compressing == 1)
			{
				compressing=2; // don't do it again
				*s++=':'; ++len;
			}

			i = fmt_xlong(s,temp);
			len += i;
			s += i;

			if (j<14)
			{
				*s++ = ':';
				++len;
			}
		}
	}

	if(compressing == 1)
	{
		*s++=':';
		++len;
	}

	*s=0;
	return len;
}

const char* inet_ntop(int AF, const void *CP, char *BUF, size_t LEN)
{
	char buf[100];
	size_t len;

	if (AF==AF_INET)
	{
		inet_ntoa_r(*(struct in_addr*)CP,buf);
		len=strlen(buf);
	} else if (AF==AF_INET6)

	{
		len=fmt_ip6(buf,(char *)CP);
	} else return 0;

	if (len<LEN)
	{
		strcpy(BUF,buf);
		return BUF;
	}
	return 0;
}
#endif

bool kvi_binaryIpToStringIp_V6(struct in6_addr in,QString &szBuffer)
{
	char buf[46];
	bool bRet =  inet_ntop(AF_INET6,(void *)&in,buf,46);

	szBuffer= buf;
	return bRet;
}

#endif

#include <errno.h>

bool kvi_select(int fd,bool * bCanRead,bool * bCanWrite,int iUSecs)
{
	// FIXME: This stuff should DIE!
	fd_set rs;
	fd_set ws;
	FD_ZERO(&rs);
	FD_ZERO(&ws);
	FD_SET(fd,&rs);
	FD_SET(fd,&ws);
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = iUSecs;

	int ret = select(fd + 1,&rs,&ws,0,&tv);
	if(ret < 1)return false; // EINTR or ENOSTUFFATALL

	*bCanRead = FD_ISSET(fd,&rs);
	*bCanWrite = FD_ISSET(fd,&ws);

	return true;
}

namespace KviNetUtils
{
	bool stringIpToBinaryIp(const QString &szStringIp,struct in_addr * address)
	{
#ifndef HAVE_INET_ATON
		QString szAddr = szStringIp.simplified();
		quint32 iAddr=0;
		QStringList ipv4 = szAddr.split(".", QString::KeepEmptyParts, Qt::CaseInsensitive);
		if (ipv4.count() == 4) {
			int i = 0;
			bool ok = TRUE;
			while(ok && i < 4) {
				uint byteValue = ipv4[i].toUInt(&ok);
				if ( (byteValue > 255) && ok )
					ok = FALSE;
				if (ok)
					iAddr = (iAddr << 8) + byteValue;
				++i;
			}
			if (ok)
			{
				if(address)address->s_addr = htonl(iAddr);
				return true;
			}
		}
		return FALSE;
#else //HAVE_INET_ATON
		if(szStringIp.isEmpty())return false;
		return (inet_aton(KviQString::toUtf8(szStringIp).data(),address) != 0);
#endif //HAVE_INET_ATON
	}


	bool isValidStringIp(const QString &szIp)
	{
		struct in_addr address;
		if(szIp.isEmpty())return false;
		if(!szIp[0].isNumber())return false;
		return stringIpToBinaryIp(szIp,&address);
	}


#ifdef COMPILE_IPV6_SUPPORT
	bool stringIpToBinaryIp_V6(const QString &szStringIp,struct in6_addr * address)
	{
		return (inet_pton(AF_INET6,KviQString::toUtf8(szStringIp).data(),(void *)address) == 1);
	}

	bool isValidStringIPv6(const QString &szIp)
	{
		struct in6_addr address;
		if(szIp.isEmpty())return false;
		return stringIpToBinaryIp_V6(szIp,&address);
	}

	bool binaryIpToStringIp_V6(struct in6_addr in,QString &szBuffer)
	{
		char buf[46];
		bool bRet =  inet_ntop(AF_INET6,(void *)&in,buf,46);
		szBuffer= buf;
		return bRet;
	}

#endif //COMPILE_IPV6_SUPPORT

	bool binaryIpToStringIp(struct in_addr in,QString &szBuffer)
	{
		char * ptr = inet_ntoa(in);
		if(!ptr)return false;
		szBuffer = ptr;
		return true;
	}

	bool isRoutableIpString(const QString &szIpString)
	{
		struct in_addr a;
		if(szIpString.isEmpty())return false;
		stringIpToBinaryIp(szIpString,&a);
		return isRoutableIp((const char *)&a);
	}

	bool isRoutableIp(const char * ipaddr)
	{
		if(!ipaddr)return false;
		const unsigned char * ip = (const unsigned char *)ipaddr;
		if(ip[0] == 0)return false;    // old-style broadcast
		if(ip[0] == 10)return false;   // Class A VPN
		if(ip[0] == 127)return false;   // loopback
		if((ip[0] == 172) && (ip[1] >= 16) && (ip[1] <= 31))return false; // Class B VPN
		if((ip[0] == 192) && (ip[1] == 168))return false; // Class C VPN
		if((ip[0] == 169) && (ip[1] == 254))return false; // APIPA
		if((ip[0] == 192) && (ip[1] == 0) && (ip[2] == 2))return false; // Class B VPN
		if(ip[0] >= 224)return false; // class D multicast and class E reserved

		return true;
	}

#ifdef COMPILE_GET_INTERFACE_ADDRESS
	union sockaddr_union {
		struct sockaddr sa;
		struct sockaddr_in sin;
	};

	bool getInterfaceAddress(const QString &szInterfaceName,QString &szBuffer)
	{
		union sockaddr_union *su;
		struct ifreq ifr;
		int len = szInterfaceName.length();
		if(len > (IFNAMSIZ - 1))return false; // invalid interface anyway

		kvi_memmove(ifr.ifr_name,KviQString::toUtf8(szInterfaceName).data(),len + 1);

		int fd = socket(AF_INET,SOCK_STREAM,0);
		if(fd < 0)return false;

		if(ioctl(fd,SIOCGIFADDR,&ifr) == -1)return false; // supports only IPV4 ?

		close(fd);

		su = (union sockaddr_union *)&(ifr.ifr_addr);

		if (su->sa.sa_family != AF_INET) return false;

		return binaryIpToStringIp((struct in_addr)su->sin.sin_addr,szBuffer);

		// (this seems to work for AF_INET only anyway)
#else //!COMPILE_GET_INTERFACE_ADDRESS
	bool getInterfaceAddress(const QString &,QString &)
	{
		return false;
#endif //!COMPILE_GET_INTERFACE_ADDRESS
	}

	void formatNetworkBandwidthString(QString &szBuffer,unsigned int uBytesPerSec)
	{
		if(uBytesPerSec > (1024 * 1024))
		{
			unsigned int uMB = uBytesPerSec / (1024 * 1024);
			unsigned int uRem = ((uBytesPerSec % (1024 * 1024)) * 100) / (1024 * 1024);

			KviQString::sprintf(szBuffer,"%u.%u%u MiB/s",uMB,uRem / 10,uRem % 10);

			return;
		}

		if(uBytesPerSec >= 1024)
		{
			unsigned int uKB = uBytesPerSec / 1024;
			unsigned int uRem = ((uBytesPerSec % 1024) * 100) / 1024;

			KviQString::sprintf(szBuffer,"%u.%u%u KiB/s",uKB,uRem / 10,uRem % 10);

			return;
		}

		KviQString::sprintf(szBuffer,"%u B/s",uBytesPerSec);

	}
}

bool kvi_isRoutableIpString(const char * ipstring)
{
	struct in_addr a;
	if(!ipstring)return false;
	kvi_stringIpToBinaryIp(ipstring,&a);
	return kvi_isRoutableIp((const char *)&a);
}

bool kvi_isRoutableIp(const char * ipaddr)
{
	if(!ipaddr)return false;
	const unsigned char * ip = (const unsigned char *)ipaddr;

	if(ip[0] == 0)return false;    // old-style broadcast
	if(ip[0] == 10)return false;   // Class A VPN
	if(ip[0] == 127)return false;   // loopback
	if((ip[0] == 172) && (ip[1] >= 16) && (ip[1] <= 31))return false; // Class B VPN
	if((ip[0] == 192) && (ip[1] == 168))return false; // Class C VPN
	if((ip[0] == 169) && (ip[1] == 254))return false; // APIPA
	if((ip[0] == 192) && (ip[1] == 0) && (ip[2] == 2))return false; // Class B VPN
	if(ip[0] >= 224)return false; // class D multicast and class E reserved

	return true;
}

bool kvi_getLocalHostAddress(QString &buffer)
{
	// This will work only on windoze...
	char buf[1024];
	if(gethostname(buf,1024) != 0)return false;
	struct hostent * h = gethostbyname(buf);
	if(!h)return false;
	QString tmp;
	int i=0;
	while(h->h_addr_list[i])
	{
		if(kvi_binaryIpToStringIp(*((struct in_addr *)(h->h_addr_list[i])),tmp))
		{
			if(kvi_isRoutableIp(h->h_addr_list[i]))
			{
				buffer = tmp;
				return true;
			}
		}
		i++;
	}

	buffer = tmp;
	return true;
}

KviSockaddr::KviSockaddr(const char * szIpAddress,kvi_u32_t uPort,bool bIPv6,bool bUdp)
{
	struct addrinfo hints;
	kvi_memset((void *)&hints,0,sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST;
#ifdef COMPILE_IPV6_SUPPORT
	hints.ai_family = bIPv6 ? PF_INET6 : PF_INET;
#else
	hints.ai_family = PF_INET;
#endif

	hints.ai_socktype = bUdp ? SOCK_DGRAM : SOCK_STREAM;

	hints.ai_protocol = 0;
	m_pData = 0;
	KviStr szPort(KviStr::Format,"%u",uPort);
	getaddrinfo(szIpAddress,szPort.ptr(),&hints,&m_pData);
}

KviSockaddr::KviSockaddr(kvi_u32_t uPort,bool bIPv6,bool bUdp) // passive sockaddr

{
	struct addrinfo hints;
	kvi_memset((void *)&hints,0,sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
#ifdef COMPILE_IPV6_SUPPORT
	hints.ai_family = bIPv6 ? PF_INET6 : PF_INET;
#else
	hints.ai_family = PF_INET;
#endif
	hints.ai_socktype = bUdp ? SOCK_DGRAM : SOCK_STREAM;
	hints.ai_protocol = 0;
	m_pData = 0;
	KviStr szPort(KviStr::Format,"%u",uPort);
	getaddrinfo(0,szPort.ptr(),&hints,&m_pData);
}

KviSockaddr::~KviSockaddr()
{
	if(m_pData)
	{
		freeaddrinfo(m_pData);
		m_pData = 0;
	}
}

struct sockaddr * KviSockaddr::socketAddress()
{
	if(!m_pData)return 0;
	return (m_pData)->ai_addr;
}

size_t KviSockaddr::addressLength()
{
	if(!m_pData)return 0;
	return (m_pData)->ai_addrlen;
}

int KviSockaddr::addressFamily()
{
	if(!m_pData)return 0;
	return (m_pData)->ai_family;
}

bool KviSockaddr::isIPv6()
{
	if(!m_pData)return false;
#ifdef COMPILE_IPV6_SUPPORT
	return false;
#else
	return (addressFamily() == AF_INET6);
#endif
}

kvi_u32_t KviSockaddr::port()
{
	if(!m_pData)return 0;
#ifdef COMPILE_IPV6_SUPPORT
	switch(m_pData->ai_family)
	{
		case AF_INET:
			return ntohs(((struct sockaddr_in *)(m_pData->ai_addr))->sin_port);
			break;
		case AF_INET6:
			return ntohs(((struct sockaddr_in6 *)(m_pData->ai_addr))->sin6_port);
			break;
	}
	return 0;
#else
	return ntohs(((struct sockaddr_in *)(m_pData->ai_addr))->sin_port);
#endif
}

bool KviSockaddr::getStringAddress(QString &szBuffer)
{
	if(!m_pData)return 0;
#ifdef COMPILE_IPV6_SUPPORT
	switch(((struct addrinfo *)m_pData)->ai_family)
	{
		case AF_INET:
			return kvi_binaryIpToStringIp(((struct sockaddr_in *)(m_pData->ai_addr))->sin_addr,szBuffer);
			break;
		case AF_INET6:
			return kvi_binaryIpToStringIp_V6(((struct sockaddr_in6 *)(m_pData->ai_addr))->sin6_addr,szBuffer);
			break;
	}

	return false;
#else
	return kvi_binaryIpToStringIp(((struct sockaddr_in *)(m_pData->ai_addr))->sin_addr,szBuffer);
#endif
}