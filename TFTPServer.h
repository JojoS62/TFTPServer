/*
 * TFTPServer.h
 * Simple TFTP server 
 *
 * Copyright (c) 2011 Jaap Vermaas
 * Modified by Zoltan Hudak 2018 for MBED-OS5
 *
 *   This file is part of the LaOS project (see: http://wiki.laoslaser.org
 *
 *   LaOS is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   LaOS is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with LaOS.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Minimal TFTP Server
 *      * Receive and send files via TFTP
 *      * Server handles only one transfer at a time
 *      * Supports only octet (raw 8 bit bytes) mode transfers
 *      * fixed block size: 512 bytes
 *
 * http://spectral.mscs.mu.edu/RFC/rfc1350.html
 *
 * Example:
 * @code 
 * TFTPServer *server;
 * ...
 * server = new TFTPServer();
 * ...
 * @endcode
 *
 */
#ifndef _TFTPSERVER_H_
#define _TFTPSERVER_H_

#include "mbed.h"

using namespace mbed;

#define TFTP_PORT   69

class TFTPServer
{
public:
    enum State
    {
        LISTENING,
        READING,
        WRITING,
        ERROR,
        SUSPENDED,
        DELETED
    };

    // Creates a new TFTP server listening on myPort.
    TFTPServer(NetworkInterface* net, uint16_t myPort = TFTP_PORT);
    
    // Destroys this instance of the TFTP server.
    ~TFTPServer();
    
    // Resets the TFTP server.
    void            reset();
    
    // Gets current TFTP status
    State           getState();
    
    // Temporarily disables incoming TFTP connections.
    void            suspend();
    
    // Resumes incoming TFTP connections after suspension.
    void            resume();
    
    // Polls for data or new connection.
    void            poll();
    
    // Gets the filename during read and write. 
    void            getFileName(char* name);
    
    // Returns number of received files.
    int             fileCount();
    
private:
    // Creates a new connection reading a file from server.
    void            connectRead(char* buff);
    
    // Creates a new connection writing a file to the server.
    void            connectWrite(char* buff);
    
    // Gets DATA block from file on disk into memory.
    void            getBlock();
    
    // Sends DATA block to remote client.
    void            sendBlock();
    
    // Compares host's IP and Port with connected remote machine.
    int             cmpHost();
    
    // Sends ACK to remote client.
    void            ack(int val);
    
    // Sends ERROR message to remote client.
    void            sendError(const char* msg);
    
    // Checks if connection mode of client is octet/binary.
    int             modeOctet(char* buff);
    
    uint16_t        port;                       // TFTP port
    UDPSocket*      socket;                     // Main listening socket (dflt: UDP port 69)
    State           state;                      // Current TFTP server state
    SocketAddress   remoteAddr;                 // Connected remote Host IP
    //int             remotePort;                 // Connected remote Host Port
    uint16_t        blockCounter, dupCounter;   // Block counter, and DUP counter
    FILE*           file;                       // Current file to read or write
    char            blockBuff[516];             // Current DATA block;
    int             blockSize;                  // Last DATA block size while sending
    char            fileName[260];              // Current (or most recent) filename
    int             fileCounter;                // Received file counter
    char            errorBuff[128];             // Error message buffer
    SocketAddress   socketAddr;                 // Socket's addres (used to get remote host's address)
};
#endif
