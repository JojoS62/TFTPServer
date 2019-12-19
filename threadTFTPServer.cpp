/* 
 * Copyright (c) 2019 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "threadTFTPServer.h"

#define STACKSIZE   (4 * 1024)
#define THREADNAME  "TFTPServer"

ThreadTFTPServer::ThreadTFTPServer(int pollingInterval) :
    _thread(osPriorityNormal, STACKSIZE, nullptr, THREADNAME),
    _cycleTime(pollingInterval)
{
}

/*
    start() : starts the thread
*/
void ThreadTFTPServer::start(NetworkInterface* net, uint16_t myPort)
{
    if(_running)
        return;

    _network = net;
    _port = myPort;

    _running = true;
    _thread.start( callback(this, &ThreadTFTPServer::myThreadFn) );
}


/*
    start() : starts the thread
*/
void ThreadTFTPServer::myThreadFn()
{
    // thread local objects
    // take care of thread stacksize !
    // DigitalOut led1(LED1);

    printf("TFTPServer starting...\n");
    _tftpServer = new TFTPServer(_network, _port);
    if(_tftpServer == nullptr){
        printf("Error: creating TFTPServer failed\n");
        return;
    }

    while(_running) {
        uint64_t nextTime = get_ms_count() + _cycleTime;

        _tftpServer->poll();

        ThisThread::sleep_until(nextTime);
    }
}

