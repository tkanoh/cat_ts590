/*
 * Copyright (c) 2014
 *      Tamotsu Kanoh <kanoh@kanoh.org>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither my name nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/wait.h>
#include "ts590.h"

#ifndef TTY_DEV
#define	TTY_DEV		"/dev/tty00"
#endif

#ifndef B_TS590
#define B_TS590		B9600
#endif

#define TIMEOUT		2

#define HZ		1.0
#define KHZ		1000.0
#define MHZ		1000000.0

#define	DEF_UNIT	KHZ
#define	UNIT_STR	"KHz"

static char *md_str[] = {"NONE",
			"LSB",
			"USB",
			"CW",
			"FM",
			"AM",
			"FSK",
			"CW-R",
			"NONE",
			"FSK-R",
			""
};

#define DEF_MODE	MODE_AM
#define DEF_VFO		'A'

#define KEY_TEXTSIZ	24
#define KEY_BUFSIZ	30 
#define KEY_BUF		10

static char *sw_str[] = {"OFF", "ON", ""};

static jmp_buf env;

void usages(progname)
char *progname;
{
	printf("usages: %s [-v vfo] [-f freq] [-m mode] [-j ref_band] [-a ant] [-k 'text'] [-s 'wpm'] [-q] [-t]\n",progname);
	exit(0);
}

void p_error(progname,msg,p)
char *progname, *msg;
int p;
{
	char buf[BUFSIZ];
	if(p) {
		snprintf(buf,sizeof(char)*BUFSIZ,"%s (error): %s ",progname,msg);
		perror(buf);
	}
	else
		fprintf(stderr,"%s (error): %s\n",progname,msg);
	exit(1);
}

int str2freq(str)
char *str;
{
	char *buf, *p;
	float unit, freq;

	buf=(char *)malloc(sizeof(char)*BUFSIZ);
	p=buf;

	while(*str != 0x0 && 
		((*str > 0x2f && *str < 0x3a) || *str == '.' || *str == ',')) {
		if(*str != ',') *p++ = *str;
		str++;
	}
	*p = 0x0;
	
	unit = DEF_UNIT;
	if(*str == 'H' || *str == 'h') unit = HZ;
	else if(*str == 'K' || *str == 'k') unit = KHZ;
	else if(*str == 'M' || *str == 'm') unit = MHZ;

	sscanf(buf,"%f",&freq);
	freq *= unit;
	free(buf);
	return((int)freq);
}

int str2mode(str)
char *str;
{
	int i;
	char *buf, *p;

	buf=(char *)malloc(sizeof(char)*BUFSIZ);
	p=buf;

	while(*str != 0x0) {
		*p++ = toupper(*str);
		str++;
	}
	*p = 0x0;

	i = 0;
	while(md_str[i] != "") {
		if(strncmp(md_str[i],buf,strlen(buf)) == 0) return(i);
		i++;
	}
	return(-1);
}

int str2sw(str)
char *str;
{
	int i;
	char *buf, *p;

	buf=(char *)malloc(sizeof(char)*BUFSIZ);
	p=buf;

	while(*str != 0x0) {
		*p++ = toupper(*str);
		str++;
	}
	*p = 0x0;

	i = 0;
	while(sw_str[i] != "") {
		if(strncmp(sw_str[i],buf,strlen(buf)) == 0) return(i);
		i++;
	}
	return(-1);
}

int get_bp_mode(freq)
int freq;
{
	int i;

	i=0;
	while(band_plan[i].start) {
		if(freq < band_plan[i].start)  return(DEF_MODE);
		if(freq >= band_plan[i].start && freq < band_plan[i].end)
			return(band_plan[i].mode);
		i++;
	}
}

int get_ibp(str)
char *str;
{
	int i;

	i=0;
	while(ibp[i].mode) {
		if(strncmp(ibp[i].band,str,strlen(ibp[i].band)) == 0) return(i);
		i++;
	}
	return(-1);
}

int wait_do_cmd(fd)
int fd;
{
	unsigned char c;

	do {
		read(fd,(char *)&c,1);
	} while(c != ';');

	return(1);
}

void cmd_write(fd,buf)
int fd;
char *buf;
{
	write(fd,buf,strlen(buf));
	wait_do_cmd(fd);
	tcflush(fd,TCIOFLUSH);
	return;
}

void key_cmd(fd,key_text)
int fd;
char *key_text;
{
	int i,j,k,n; 
	char *key_buf[KEY_BUF], *key_buf_p[KEY_BUF], buf[KEY_BUFSIZ];
	
	for(i=0;i < KEY_BUF;i++)
		key_buf[i]=key_buf_p[i]=(char *)malloc(sizeof(char)*KEY_BUFSIZ);

	for(i=0; i<KEY_BUF; i++) {
		*key_buf_p[i]++ = 'K';
		*key_buf_p[i]++ = 'Y';
		*key_buf_p[i]++ = ' ';
		j=0; k=0;
		while(*key_text != '\0' && j < KEY_TEXTSIZ) {
			if(*key_text == ' ') k=j;
			*key_buf_p[i]++ = *key_text++;
			j++;
		}
		if(*key_text == '\0') {
			for(n=j;n<KEY_TEXTSIZ;n++)
				*key_buf_p[i]++ = ' ';
		}
		else {
			for(n=k;n<KEY_TEXTSIZ;n++) {
				key_text--;
				key_buf_p[i]--;
			}
			key_text++;
			for(n=k;n<KEY_TEXTSIZ;n++)
				*key_buf_p[i]++ = ' ';
		}
		*key_buf_p[i]++ = ';';
		*key_buf_p[i]++ = ';';
		*key_buf_p[i] = '\0';
		if(*key_text == '\0') {
			n=i+1;
			break;
		}
	}

	for(i=0;i<n;i++) {
		cmd_write(fd,key_buf[i]);
		snprintf(buf,sizeof(char)*KEY_BUFSIZ,"%a\n",key_buf[i]);
	}

	for(i=0;i < KEY_BUF;i++) free(key_buf[i]);
	return;
}

void static system_timeout(sig)
int sig;
{
	siglongjmp(env, 1);
}

int main(argc, argv)
int argc;
char **argv;
{
	int i, fd, len, freq, mode, vfo, fine, ibp_n, sql, keying, wpm, abort_keying, ant, ant_port, ant_rx, tune;
	int status, exit_code;
	static pid_t pid;
	char buf[BUFSIZ], *key_text;
	struct termios term, term_def;
	extern char *optarg;

	key_text=(char *)malloc(sizeof(char)*BUFSIZ);
	
	vfo = DEF_VFO;

	if(argc < 2) usages(argv[0]);

	freq = 0; mode = 0; ibp_n = -1; keying=0; wpm=0; abort_keying=0; ant=0; ant_port=1; tune=0;
	while ((i = getopt(argc, argv, "v:f:m:j:k:a:qs:th?")) != -1) {
		switch (i) {
			case 'v':
				if(*optarg == 'b' || *optarg == 'B') vfo = 'B';
				break;
			case 'f':
				freq = str2freq(optarg);
				if(freq > MAX_FREQ || freq < MIN_FREQ)
					p_error(argv[0],"Out of Band",0);
				break;
			case 'm':
				mode = str2mode(optarg);
				if(mode < 0)
					p_error(argv[0],"illgale mode",0);
				break;
			case 'j':
				ibp_n = get_ibp(optarg); 
				if(ibp_n < 0)
					p_error(argv[0],"No beacon station in band",0);
				break;
			case 'k':
				key_text = optarg;
				keying = 1;
				break;
			case 'a':
				ant=1;
				ant_port=9;
				ant_rx=9;
				if(*optarg == '1') ant_port=1;
				if(*optarg == '2') ant_port=2;
				if(*optarg == '0') ant_rx=0;
				if(*optarg == '3') ant_rx=1;
				break;
			case 's':
				wpm=atoi(optarg);
				break;
			case 't':
				tune=1;
				break;
			case 'q':
				abort_keying=1; 
				break;
			case 'h':
			case '?':
			default:
				usages(argv[0]);
		}
	}

	if(ibp_n > -1) {
		freq = ibp[ibp_n].freq;
		mode = ibp[ibp_n].mode;
	}

	if(freq && !mode)
		mode = get_bp_mode(freq);

	if(mode == MODE_AM || mode == MODE_FM)
		fine = 0;
	else
		fine = 1;

	if(mode == MODE_FM)
		sql = 128;
	else
		sql = 0;

	if(sigsetjmp(env, 1)) {
		alarm(0);
		signal(SIGALRM, SIG_DFL);
		if(pid > 0)
			kill(pid, SIGKILL);
		p_error(argv[0],"CAT access timeout",0);
	}

	pid=fork();
	switch(pid) {
		case -1:
			p_error(argv[0],buf,1);
			break;
		case 0:
			if((fd = open(TTY_DEV, O_RDWR | O_NONBLOCK)) < 0)
				p_error(argv[0],TTY_DEV,1);

			tcgetattr(fd,&term_def);

			term.c_iflag = IGNBRK | IGNPAR | IMAXBEL;
			term.c_oflag = 0;
			term.c_cflag = CS8 | CREAD | CLOCAL | CRTSCTS;
			term.c_lflag = 0;
			term.c_cc[VMIN] = 1;
			term.c_cc[VTIME] = 0;

			cfsetispeed(&term,B_TS590);
			cfsetospeed(&term,B_TS590);

			tcflush(fd,TCIOFLUSH);
			tcsetattr(fd, TCSANOW, &term);

			if(mode) {
				snprintf(buf,sizeof(char)*BUFSIZ,"MD%d;;",mode);
				cmd_write(fd,buf);
				snprintf(buf,sizeof(char)*BUFSIZ,"SQ%04d;;",sql);
				cmd_write(fd,buf);
			}

			if(freq) {
				snprintf(buf,sizeof(char)*BUFSIZ,"F%c000%08d;;",vfo,freq);
				cmd_write(fd,buf);
				snprintf(buf,sizeof(char)*BUFSIZ,"FS%1d;;",fine);
				cmd_write(fd,buf);
			} 

			if(wpm) {
				snprintf(buf,sizeof(char)*BUFSIZ,"KS%03d;;",wpm);
				cmd_write(fd,buf);
			}

			if(keying)
				key_cmd(fd,key_text);

			if(abort_keying) {
				snprintf(buf,sizeof(char)*BUFSIZ,"KY0;;");
				cmd_write(fd,buf);
			}

			if(ant) {
				snprintf(buf,sizeof(char)*BUFSIZ,"AN%1d%1d9;;",ant_port,ant_rx);
				cmd_write(fd,buf);
			}

			if(tune) {
				snprintf(buf,sizeof(char)*BUFSIZ,"AC111;;");
				cmd_write(fd,buf);
			}
		

			tcsetattr(fd, TCSANOW, &term_def);
			close(fd);
			_exit(0);
			break;
		default:
			alarm(TIMEOUT);
			signal(SIGALRM, system_timeout);
			pid = waitpid(pid, &status, WUNTRACED);
			exit_code = WEXITSTATUS(status);
	}
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	exit(exit_code);
}
