/**
  ******************************************************************************
  * @file           : SERIAL.c
  * @brief          : 
  * @author         : UF4OVER
  * @date           : 2025/11/14
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 UF4.
  * All rights reserved.
  *
  ******************************************************************************
  */
//
// Created by 33974 on 2025/11/14.
//

/* Includes ------------------------------------------------------------------*/
#include "SERIAL.h"
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
/* Exclude unrelated Windows subsystems to avoid pulling COM/COMDLG, etc. */
#ifndef NOWINSCARD
#define NOWINSCARD
#endif
#ifndef NOCRYPT
#define NOCRYPT
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOHELP
#define NOHELP
#endif
#ifndef NOMCX
#define NOMCX
#endif
#ifndef NOIME
#define NOIME
#endif
#ifndef NOSERVICE
#define NOSERVICE
#endif
#ifndef NOMCX
#define NOMCX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/types.h>
#endif

struct serial_t {
#ifdef _WIN32
  HANDLE h;
#else
  int fd;
#endif
};

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

serial_t *serial_open(const char *portname, unsigned int baud)
{
    if (!portname) return NULL;
    serial_t *s = (serial_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;

#ifdef _WIN32
    char fullname[256];
    if (strncmp(portname, "\\\\.\\", 4) == 0) {
        snprintf(fullname, sizeof(fullname), "%s", portname);
    } else {
        snprintf(fullname, sizeof(fullname), "\\\\.\\%s", portname);
    }

    s->h = CreateFileA(fullname, GENERIC_READ | GENERIC_WRITE,
                       0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (s->h == INVALID_HANDLE_VALUE) {
        free(s);
        return NULL;
    }

    DCB dcb;
    ZeroMemory(&dcb, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(s->h, &dcb)) {
        CloseHandle(s->h);
        free(s);
        return NULL;
    }
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fBinary = TRUE;
    if (!SetCommState(s->h, &dcb)) {
        CloseHandle(s->h);
        free(s);
        return NULL;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;
    SetCommTimeouts(s->h, &timeouts);

    PurgeComm(s->h, PURGE_RXCLEAR | PURGE_TXCLEAR);

#else
    int fd = open(portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        free(s);
        return NULL;
    }

    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) {
        close(fd);
        free(s);
        return NULL;
    }

    cfmakeraw(&tio);

    speed_t speed = B115200;
    switch (baud) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
#ifdef B230400
        case 230400: speed = B230400; break;
#endif
        default: speed = B115200; break;
    }
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;

    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1; /* 0.1s */

    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        free(s);
        return NULL;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);

    s->fd = fd;
#endif

    return s;
}

ssize_t serial_write(serial_t *s, const void *buf, size_t len)
{
    if (!s || !buf) return -1;
#ifdef _WIN32
    DWORD written = 0;
    if (!WriteFile(s->h, buf, (DWORD)len, &written, NULL)) return -1;
    return (ssize_t)written;
#else
    ssize_t n = write(s->fd, buf, len);
    return n;
#endif
}

ssize_t serial_read(serial_t *s, void *buf, size_t len, unsigned int timeout_ms)
{
    if (!s || !buf) return -1;
#ifdef _WIN32
    COMMTIMEOUTS old;
    if (!GetCommTimeouts(s->h, &old)) return -1;
    COMMTIMEOUTS to = old;
    to.ReadIntervalTimeout = timeout_ms ? timeout_ms : 50;
    to.ReadTotalTimeoutMultiplier = 0;
    to.ReadTotalTimeoutConstant = timeout_ms;
    SetCommTimeouts(s->h, &to);

    DWORD readBytes = 0;
    BOOL ok = ReadFile(s->h, buf, (DWORD)len, &readBytes, NULL);
    SetCommTimeouts(s->h, &old);
    if (!ok) return -1;
    return (ssize_t)readBytes;
#else
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(s->fd, &rfds);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int rv = select(s->fd + 1, &rfds, NULL, NULL, timeout_ms ? &tv : NULL);
    if (rv < 0) return -1;
    if (rv == 0) return 0;
    ssize_t n = read(s->fd, buf, len);
    return n;
#endif
}

void serial_close(serial_t *s)
{
    if (!s) return;
#ifdef _WIN32
    if (s->h && s->h != INVALID_HANDLE_VALUE) CloseHandle(s->h);
#else
    if (s->fd >= 0) close(s->fd);
#endif
    free(s);
}

/* USER CODE END 0 */

/* External functions --------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

/* Exported functions --------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
