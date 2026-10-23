// Copyright (C) 2025 nihui&futz12
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "writer_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#endif

#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#endif

namespace cv {

class writer_http_client
{
public:
#ifdef _WIN32
    writer_http_client(SOCKET _cfd) : cfd(_cfd) {}
    SOCKET cfd;
#else
    writer_http_client(int _cfd) : cfd(_cfd) {}
    int cfd;
#endif

public:
    bool valid()
    {
#ifdef _WIN32
        return cfd != INVALID_SOCKET;
#else
        return cfd >= 0;
#endif
    }

    void close()
    {
#ifdef _WIN32
        closesocket(cfd);
        cfd = INVALID_SOCKET;
#else
        ::close(cfd);
        cfd = -1;
#endif
    }

    void wait_for_recv()
    {
        if (!valid())
            return;

        fd_set rfds;
        FD_ZERO(&rfds);
#ifdef _WIN32
        FD_SET(cfd, &rfds);
#else
        FD_SET(cfd, &rfds);
#endif

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

#ifdef _WIN32
        int ret = select(0, &rfds, 0, 0, &tv);
#else
        int ret = select(cfd + 1, &rfds, 0, 0, &tv);
#endif
        if (ret <= 0)
        {
            close();
        }
    }

    void wait_for_send()
    {
        if (!valid())
            return;

        fd_set wfds;
        FD_ZERO(&wfds);
#ifdef _WIN32
        FD_SET(cfd, &wfds);
#else
        FD_SET(cfd, &wfds);
#endif

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

#ifdef _WIN32
        int ret = select(0, 0, &wfds, 0, &tv);
#else
        int ret = select(cfd + 1, 0, &wfds, 0, &tv);
#endif
        if (ret <= 0)
        {
            close();
        }
    }

    void recv(char* buf, size_t len)
    {
        wait_for_recv();

        if (!valid())
            return;

#ifdef _WIN32
        int nrecv = ::recv(cfd, buf, (int)len, 0);
#else
        ssize_t nrecv = ::recv(cfd, buf, len, 0);
#endif
        if (nrecv <= 0)
        {
            buf[0] = '\0';
            close();
        }
        else
        {
            buf[nrecv] = '\0';
        }
    }

    void send(const char* buf, size_t len)
    {
        wait_for_send();

        if (!valid())
            return;

#ifdef _WIN32
        int nsend = ::send(cfd, buf, (int)len, 0);
#else
        ssize_t nsend = ::send(cfd, buf, len, MSG_NOSIGNAL);
#endif
        if (nsend != (int)len)
        {
            close();
        }
    }
};

class writer_http_impl
{
public:
    writer_http_impl();
    ~writer_http_impl();

    int open(int port);

    void write_jpgbuf(const std::vector<unsigned char>& jpgbuf);

    void close();

public:
#ifdef _WIN32
    SOCKET sfd;
    HANDLE looper;
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE cond;
#else
    int sfd;
    pthread_t looper;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
#endif

    int port;

    void mainloop();

    std::vector<unsigned char> jpgbuf;
    const unsigned char* jpg_data;
    int jpg_size;
    int exit_flag;
};

#ifdef _WIN32
static DWORD WINAPI server_loop(LPVOID args)
#else
static void* server_loop(void* args)
#endif
{
    ((writer_http_impl*)args)->mainloop();
    return 0;
}

writer_http_impl::writer_http_impl()
{
#ifdef _WIN32
    sfd = INVALID_SOCKET;
    looper = NULL;
    InitializeCriticalSection(&mutex);
    InitializeConditionVariable(&cond);
#else
    sfd = -1;
    looper = 0;
    pthread_mutex_init(&mutex, 0);
    pthread_cond_init(&cond, 0);
#endif

    port = 7766;

    jpg_data = 0;
    jpg_size = 0;
    exit_flag = 0;
}

writer_http_impl::~writer_http_impl()
{
    close();
}

int writer_http_impl::open(int _port)
{
    port = _port;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return -1;
    }
#endif

#ifdef _WIN32
    looper = CreateThread(NULL, 0, server_loop, this, 0, NULL);
    if (looper == NULL)
#else
    int ret = pthread_create(&looper, 0, server_loop, (void*)this);
    if (ret != 0)
#endif
    {
        fprintf(stderr, "Thread creation failed\n");
        return -1;
    }

    return 0;
}

void writer_http_impl::write_jpgbuf(const std::vector<unsigned char>& _jpgbuf)
{
    if (!looper)
        return;

#ifdef _WIN32
    EnterCriticalSection(&mutex);
    while (jpg_data)
    {
        SleepConditionVariableCS(&cond, &mutex, INFINITE);
    }

    jpgbuf = _jpgbuf;
    jpg_data = jpgbuf.data();
    jpg_size = (int)jpgbuf.size();
    LeaveCriticalSection(&mutex);
    WakeConditionVariable(&cond);
#else
    pthread_mutex_lock(&mutex);
    while (jpg_data)
    {
        pthread_cond_wait(&cond, &mutex);
    }

    jpgbuf = _jpgbuf;
    jpg_data = jpgbuf.data();
    jpg_size = (int)jpgbuf.size();
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);
#endif
}

void writer_http_impl::mainloop()
{
#ifdef _WIN32
    sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    sfd = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (sfd <= 0)
        return;

#ifdef _WIN32
    int on = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
#else
    int on = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif

    // bind
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
            goto EXIT;
    }

    // listen
    if (listen(sfd, 8) != 0)
        goto EXIT;

    // Print streaming URLs
    {
#ifdef _WIN32
        PIP_ADAPTER_ADDRESSES pAddresses = NULL;
        ULONG outBufLen = 0;
        DWORD dwRetVal = GetAdaptersAddresses(AF_INET, 0, NULL, NULL, &outBufLen);
        if (dwRetVal == ERROR_BUFFER_OVERFLOW)
        {
            pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
            dwRetVal = GetAdaptersAddresses(AF_INET, 0, NULL, pAddresses, &outBufLen);
        }

        if (dwRetVal == NO_ERROR)
        {
            PIP_ADAPTER_ADDRESSES pCurr = pAddresses;
            while (pCurr)
            {
                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurr->FirstUnicastAddress;
                while (pUnicast)
                {
                    if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
                    {
                        char ipstr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &((struct sockaddr_in*)pUnicast->Address.lpSockaddr)->sin_addr, ipstr, sizeof(ipstr));
                        fprintf(stderr, "streaming at http://%s:%d\n", ipstr, port);
                    }
                    pUnicast = pUnicast->Next;
                }
                pCurr = pCurr->Next;
            }
        }
        free(pAddresses);
#else
        struct ifaddrs *ifaddr, *ifa;
        if (getifaddrs(&ifaddr) != -1)
        {
            for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
            {
                if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
                {
                    void* addr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
                    char addressBuffer[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, addr, addressBuffer, INET_ADDRSTRLEN);
                    fprintf(stderr, "streaming at http://%s:%d\n", addressBuffer, port);
                }
            }
            freeifaddrs(ifaddr);
        }
#endif
        fprintf(stderr, "streaming at http://127.0.0.1:%d\n", port);
    }

    while (!exit_flag)
    {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
#ifdef _WIN32
        SOCKET cfd = accept(sfd, (struct sockaddr*)&addr, &len);
#else
        int cfd = accept(sfd, (struct sockaddr*)&addr, &len);
#endif

        if (cfd <= 0)
            break;

        writer_http_client client(cfd);

        char cip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, cip, sizeof(cip));
        fprintf(stderr, "client accepted %s:%d\n", cip, ntohs(addr.sin_port));

        while (client.valid())
        {
            char buf[1024];
            client.recv(buf, sizeof(buf)-1);

            if (strstr(buf, "GET / HTTP/1.1"))
            {
                const char* header = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
                client.send(header, strlen(header));

                while (client.valid())
                {
#ifdef _WIN32
                    EnterCriticalSection(&mutex);
                    while (!jpg_data && !exit_flag)
                        SleepConditionVariableCS(&cond, &mutex, INFINITE);

                    if (exit_flag)
                    {
                        LeaveCriticalSection(&mutex);
                        break;
                    }

                    char header2[128];
                    int header_len = sprintf(header2, "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", jpg_size);
                    client.send(header2, header_len);
                    client.send((const char*)jpg_data, jpg_size);
                    client.send("\r\n", 2);

                    jpg_data = 0;
                    jpg_size = 0;
                    LeaveCriticalSection(&mutex);
                    WakeConditionVariable(&cond);
#else
                    pthread_mutex_lock(&mutex);
                    while (!jpg_data && !exit_flag)
                        pthread_cond_wait(&cond, &mutex);

                    if (exit_flag)
                    {
                        pthread_mutex_unlock(&mutex);
                        break;
                    }

                    char header2[128];
                    int header_len = sprintf(header2, "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", jpg_size);
                    client.send(header2, header_len);
                    client.send((const char*)jpg_data, jpg_size);
                    client.send("\r\n", 2);

                    jpg_data = 0;
                    jpg_size = 0;
                    pthread_mutex_unlock(&mutex);
                    pthread_cond_signal(&cond);
#endif
                }
            }
            else
            {
                const char* response = "HTTP/1.1 404 Not Found\r\n\r\n";
                client.send(response, strlen(response));
                break;
            }
        }

        fprintf(stderr, "client closed %s:%d\n", cip, ntohs(addr.sin_port));
    }

EXIT:
#ifdef _WIN32
    if (sfd != INVALID_SOCKET) closesocket(sfd);
    WSACleanup();
#else
    if (sfd >= 0) ::close(sfd);
#endif
    sfd = -1;
}

void writer_http_impl::close()
{
    if (!looper)
        return;

#ifdef _WIN32
    EnterCriticalSection(&mutex);
    exit_flag = 1;
    LeaveCriticalSection(&mutex);
    WakeConditionVariable(&cond);
    WaitForSingleObject(looper, INFINITE);
    CloseHandle(looper);
    DeleteCriticalSection(&mutex);
#else
    pthread_mutex_lock(&mutex);
    exit_flag = 1;
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);
    pthread_join(looper, NULL);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
#endif
    looper = 0;
}

bool writer_http::supported()
{
    return true;
}

writer_http::writer_http() : d(new writer_http_impl) {}

writer_http::~writer_http()
{
    delete d;
}

int writer_http::open(int port)
{
    return d->open(port);
}

void writer_http::write_jpgbuf(const std::vector<unsigned char>& jpgbuf)
{
    d->write_jpgbuf(jpgbuf);
}

void writer_http::close()
{
    d->close();
}

} // namespace cv