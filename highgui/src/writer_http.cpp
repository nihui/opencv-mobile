//
// Copyright (C) 2024 nihui
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

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>

namespace cv {

class writer_http_client
{
public:
    writer_http_client(int _cfd) : cfd(_cfd) {}

    int cfd;

public:
    bool valid()
    {
        return cfd >= 0;
    }

    void close()
    {
        ::close(cfd);
        cfd = -1;
    }

    void wait_for_recv()
    {
        if (cfd == -1)
            return;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(cfd, &rfds);

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int ret = select(cfd + 1, &rfds, 0, 0, &tv);
        if (ret <= 0)
        {
            // timeout or error
            close();
        }
    }

    void wait_for_send()
    {
        if (cfd == -1)
            return;

        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(cfd, &wfds);

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int ret = select(cfd + 1, 0, &wfds, 0, &tv);
        if (ret <= 0)
        {
            // timeout or error
            close();
        }
    }

    void recv(char* buf, size_t len)
    {
        wait_for_recv();

        if (cfd == -1)
            return;

        ssize_t nrecv = ::recv(cfd, buf, len, 0);
        if (nrecv == -1)
        {
            buf[0] = '\0';
            // recv break
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

        if (cfd == -1)
            return;

        ssize_t nsend = ::send(cfd, buf, len, MSG_NOSIGNAL);
        if (nsend != len)
        {
            // send break
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

    void write_jpgbuf(const unsigned char* jpg_data, int size);

    void close();

public:
    int sfd;
    int port;

    pthread_t looper;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    void mainloop();

    // cv::Mat bgr_to_send;
    const unsigned char* jpg_data;
    int jpg_size;
    int exit_flag;
};

static void* server_loop(void* args)
{
    ((writer_http_impl*)args)->mainloop();
    return NULL;
}

writer_http_impl::writer_http_impl()
{
    sfd = -1;
    port = 7766;

    looper = 0;

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

    {
        int ret = pthread_mutex_init(&mutex, 0);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_mutex_init failed\n");
            return -1;
        }
    }

    {
        int ret = pthread_cond_init(&cond, 0);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_cond_init failed\n");
            return -1;
        }
    }

    {
        int ret = pthread_create(&looper, 0, server_loop, (void*)this);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_create failed\n");
            return -1;
        }
    }

    return 0;
}

void writer_http_impl::write_jpgbuf(const unsigned char* _jpg_data, int size)
{
    if (!looper)
        return;

    pthread_mutex_lock(&mutex);
    {
        while (jpg_data)
        {
            pthread_cond_wait(&cond, &mutex);
        }

        jpg_data = _jpg_data;
        jpg_size = size;
    }
    pthread_mutex_unlock(&mutex);

    pthread_cond_signal(&cond);
}

void writer_http_impl::mainloop()
{
    sfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sfd <= 0)
        return;

    {
        int on = 1;
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    }

    // bind
    {
        const char* ip = "0.0.0.0";

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip);

        if (::bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
            goto OUT;
    }

    // listen
    {
        if (::listen(sfd, 8) != 0)
            goto OUT;
    }

    // print streaming url
    {
        struct ifaddrs *ifaddr, *ifa;

        if (getifaddrs(&ifaddr) == -1)
            goto OUT;

        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == NULL)
                continue;

            int family = ifa->ifa_addr->sa_family;

            if (family == AF_INET)
            {
                void* addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, addr, addressBuffer, INET_ADDRSTRLEN);

                fprintf(stderr, "streaming at http://%s:%d\n", addressBuffer, port);
            }
        }

        freeifaddrs(ifaddr);

        fprintf(stderr, "streaming at http://127.0.0.1:%d\n", port);
    }

    while (1)
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        socklen_t len = sizeof(addr);

        int cfd = ::accept(sfd, (sockaddr*)&addr, &len);

        const char* cip = inet_ntoa(addr.sin_addr);
        int cport = ntohs(addr.sin_port);

        fprintf(stderr, "client accepted %s %d\n", cip, cport);

        writer_http_client client(cfd);

        // handle client
        while (1)
        {
            char buf[1024 * 1024];
            client.recv(buf, 1024 * 1024 - 1);

            // fprintf(stderr, "%s", buf);
            // fprintf(stderr, "-------------------------\n");

            if (memcmp(buf, "GET / HTTP/1.1", 14) == 0)
            {
                const char sbuf[] = "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: multipart/x-mixed-replace;boundary=frame\r\n"
                                    "Connection: keep-alive\r\n"
                                    "Cache-Control: no-cache, no-store, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n"
                                    "Pragma: no-cache\r\n"
                                    "\r\n";

                client.send(sbuf, sizeof(sbuf));

                client.send("--frame\r\n", 9);

                while (1)
                {
                    // wait for new image
                    pthread_mutex_lock(&mutex);
                    {
                        while (!jpg_data && !exit_flag)
                        {
                            pthread_cond_wait(&cond, &mutex);
                        }

                        // block send jpg_data
                        if (!exit_flag)
                        {
                            char sbuf2[1024];
                            sprintf(sbuf2, "Content-Type: image/jpeg\r\nContent-Length: %lu\r\n\r\n", jpg_size);
                            client.send(sbuf2, strlen(sbuf2));

                            client.send((const char*)jpg_data, jpg_size);

                            // fprintf(stderr, "[INFO] 4\n");

                            client.send("\r\n--frame\r\n", 11);
                        }

                        jpg_data = 0;
                        jpg_size = 0;
                    }
                    pthread_mutex_unlock(&mutex);

                    pthread_cond_signal(&cond);

                    if (exit_flag)
                    {
                        client.close();
                        goto OUT;
                    }

                    if (!client.valid())
                        break;
                }

                // send error or client timeout
                break;
            }
            else
            {
                const char sbuf[] = "HTTP/1.1 404 Not Found\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";

                client.send(sbuf, strlen(sbuf));

                client.close();
                break;
            }
        }

        fprintf(stderr, "client closed %s %d\n", cip, cport);
    }

OUT:
    ::close(sfd);
    sfd = -1;
}

void writer_http_impl::close()
{
    if (!looper)
        return;

    // set exit flag
    pthread_mutex_lock(&mutex);
    {
        exit_flag = 1;

        pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&mutex);

    {
        int ret = pthread_join(looper, 0);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_join failed\n");
        }
    }

    {
        int ret = pthread_cond_destroy(&cond);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_cond_destroy failed\n");
        }
    }

    {
        int ret = pthread_mutex_destroy(&mutex);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_mutex_destroy failed\n");
        }
    }

    looper = 0;

    jpg_data = 0;
    jpg_size = 0;
    exit_flag = 0;
}

bool writer_http::supported()
{
    return true;
}

writer_http::writer_http() : d(new writer_http_impl)
{
}

writer_http::~writer_http()
{
    delete d;
}

int writer_http::open(int port)
{
    return d->open(port);
}

void writer_http::write_jpgbuf(const unsigned char* jpgdata, int size)
{
    d->write_jpgbuf(jpgdata, size);
}

void writer_http::close()
{
    d->close();
}

} // namespace cv
