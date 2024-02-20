/*
 * TFTPServer.cpp
 * Simple TFTP server
 *
 * Copyright (c) 2011 Jaap Vermaas
 * Modified by Zoltan Hudak 2018 for MBED-OS5
 *
 *   This file is part of the LaOS project (see: http://wiki.laoslaser.org)
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
 */
#include "TFTPServer.h"

//#define DEBUG_TFTP
#ifdef DEBUG_TFTP
#define DEBUG_TFTP(...) printf(__VA_ARGS__)
#else
#define DEBUG_TFTP(...)
#endif

/**
 * @brief   Creates a new TFTP server listening on myPort.
 * @note
 * @param   net  A pointer to EthernetInterface object.
 * @param   port A port to listen on (defaults to 69).
 * @retval
 */
TFTPServer::TFTPServer(NetworkInterface* net, uint16_t myPort /* = 69 */ )
{
    port = myPort;
    DEBUG_TFTP("TFTPServer(): port=%d\r\n", myPort);

    socket = new UDPSocket();
    socket->open(net);
    
    state = LISTENING;
    if (socket->bind(port))
    {
        socketAddr = SocketAddress(0, port);
        state = ERROR;
    }

    DEBUG_TFTP("FTP server state = %d\r\n", getState());

    socket->set_blocking(true);

    fileCounter = 0;
}

/**
 * @brief   Destroys this instance of the TFTP server.
 * @note
 * @param
 * @retval
 */
TFTPServer::~TFTPServer()
{
    socket->close();
    delete(socket);
    state = DELETED;
}

/**
 * @brief   Resets the TFTP server.
 * @note
 * @param
 * @retval
 */
void TFTPServer::reset()
{
    socket->close();
    delete(socket);
    socket = new UDPSocket();
    state = LISTENING;
    if (socket->bind(port))
    {
        socketAddr.set_port(port);
        state = ERROR;
    }

    socket->set_blocking(false);
    strcpy(fileName, "");
    fileCounter = 0;
}

/**
 * @brief   Gets current TFTP status.
 * @note
 * @param
 * @retval
 */
TFTPServer::State TFTPServer::getState()
{
    return state;
}

/**
 * @brief   Temporarily disables incoming TFTP connections.
 * @note
 * @param
 * @retval
 */
void TFTPServer::suspend()
{
    state = SUSPENDED;
}

/**
 * @brief   Resumes incoming TFTP connection after suspension.
 * @note
 * @param
 * @retval
 */
void TFTPServer::resume()
{
    if (state == SUSPENDED)
        state = LISTENING;
}

/**
 * @brief   Polls for data or new connection.
 * @note
 * @param
 * @retval
 */
void TFTPServer::poll()
{
    if ((state == SUSPENDED) || (state == DELETED) || (state == ERROR))
        return;

    char    buff[516];
    int     len = socket->recvfrom(&socketAddr, buff, sizeof(buff));

    if (len <= 0)
        return;

    DEBUG_TFTP("Got block with size %d.\n\r", len);

    switch (state) {
        case LISTENING:
            {
                switch (buff[1]) {
                    case 0x01:          // RRQ
                        connectRead(buff);
                        break;

                    case 0x02:          // WRQ
                        connectWrite(buff);
                        break;

                    case 0x03:          // DATA before connection established
                        sendError("No data expected.\r\n");
                        break;

                    case 0x04:          // ACK before connection established
                        sendError("No ack expected.\r\n");
                        break;

                    case 0x05:          // ERROR packet received
                        DEBUG_TFTP("TFTP Error received.\r\n");
                        break;

                    default:            // unknown TFTP packet type
                        sendError("Unknown TFTP packet type.\r\n");
                        break;
                }                       // switch buff[1]
                break;                  // case listening
            }

        case READING:
            {
                if (cmpHost())
                {
                    switch (buff[1]) {
                        case 0x01:
                            // if this is the receiving host, send first packet again
                            if (blockCounter == 1)
                            {
                                ack(0);
                                dupCounter++;
                            }

                            if (dupCounter > 10)
                            {           // too many dups, stop sending
                                sendError("Too many dups");
                                fclose(file);
                                state = LISTENING;
                                remoteAddr.set_ip_address("");
                            }
                            break;

                        case 0x02:
                            // this should never happen, ignore
                            sendError("WRQ received on open read socket");
                            fclose(file);
                            state = LISTENING;
                            remoteAddr.set_ip_address("");
                            break;

                        case 0x03:
                            // we are the sending side, ignore
                            sendError("Received data package on sending socket");
                            fclose(file);
                            state = LISTENING;
                            remoteAddr.set_ip_address("");
                            break;

                        case 0x04:
                            // last packet received, send next if there is one
                            dupCounter = 0;
                            if (blockSize == 516)
                            {
                                getBlock();
                                sendBlock();
                            }
                            else
                            {           //EOF
                                fclose(file);
                                state = LISTENING;
                                remoteAddr.set_ip_address("");
                            }
                            break;

                        default:        // this includes 0x05 errors
                            sendError("Received 0x05 error message");
                            fclose(file);
                            state = LISTENING;
                            remoteAddr.set_ip_address("");
                            break;
                    }                   // switch (buff[1])
                }
                else {
                    DEBUG_TFTP("Ignoring package from other remote client during RRQ.\r\n");
                }
                break;                  // reading
            }

        case WRITING:
            {
                if (cmpHost())
                {
                    switch (buff[1]) {
                        case 0x02:
                            {
                                // if this is a returning host, send ack again
                                ack(0);
                                DEBUG_TFTP("Resending Ack on WRQ.\r\n");
                                break;  // case 0x02
                            }

                        case 0x03:
                            {
                                int block = (buff[2] << 8) + buff[3];
                                if ((blockCounter + 1) == block)
                                {
                                    ack(block);
                                    // new packet
                                    char*   data = &buff[4];
                                    fwrite(data, 1, len - 4, file);
                                    blockCounter++;
                                    dupCounter = 0;
                                }
                                else
                                {       // mismatch in block nr
                                    if ((blockCounter + 1) < block)
                                    {   // too high
                                        sendError("Packet count mismatch");
                                        fclose(file);
                                        state = LISTENING;
                                        remove(fileName);
                                        remoteAddr.set_ip_address("");
                                    }
                                    else
                                    {   // duplicate packet, send ACK again
                                        if (dupCounter > 10)
                                        {
                                            sendError("Too many dups");
                                            fclose(file);
                                            remove(fileName);
                                            state = LISTENING;
                                        }
                                        else
                                        {
                                            ack(blockCounter);
                                            dupCounter++;
                                        }
                                    }
                                }

                                if (len < 516)
                                {
                                    ack(blockCounter);
                                    fclose(file);
                                    state = LISTENING;
                                    remoteAddr.set_ip_address("");
                                    fileCounter++;
                                    DEBUG_TFTP("File receive finished.\r\n");
                                }
                                break;  // case 0x03
                            }

                        default:
                            {
                                sendError("No idea why you're sending me this!");
                                break;  // default
                            }
                    }                   // switch (buff[1])
                }
                else {
                    DEBUG_TFTP("Ignoring packege from other remote client during WRQ.\r\n");
                }
                break;                  // writing
            }

        case ERROR:
        case SUSPENDED:
        case DELETED:
        default:
            { }
    }                                   // state
}

/**
 * @brief   Gets the file name during read and write.
 * @note
 * @param   name  A pointer to C-style string to be filled with file name.
 * @retval
 */
void TFTPServer::getFileName(char* name)
{
    sprintf(name, "%s", fileName);
}

/**
 * @brief   Returns number of received files.
 * @note
 * @param
 * @retval
 */
int TFTPServer::fileCount()
{
    return fileCounter;
}

/**
 * @brief   Creates a new connection reading a file from server.
 * @note    Sends the file to the remote client.
 *          Sends en error message to the remote client in case of failure.
 * @param   buff  A char array to pass data.
 * @retval
 */
void TFTPServer::connectRead(char* buff)
{
    blockCounter = 0;
    dupCounter = 0;
    remoteAddr = socketAddr;

    sprintf(fileName, "%s", &buff[2]);

    if (modeOctet(buff))
        file = fopen(fileName, "rb");
    else
        file = fopen(fileName, "r");

    if (!file)
    {
        state = LISTENING;

        char    msg[123] = { "Could not read file: " };

        strcat(msg, fileName);
        strcat(msg, "\r\n");
        sendError(msg);
    }
    else
    {
        // file ready for reading
        state = READING;
        DEBUG_TFTP("Listening: Requested file %s from TFTP connection %s port %d\r\n",
            fileName,
            remoteAddr.get_ip_address(),
            remoteAddr.get_port()
        );
        getBlock();
        sendBlock();
    }
}

/**
 * @brief   Creates a new connection for writing a file to the server.
 * @note    Sends the file to the TFTP server.
 *          Sends error message to the remote client in case of failure.
 * @param   buff  A char array to pass data.
 * @retval
 */
void TFTPServer::connectWrite(char* buff)
{
    ack(0);
    blockCounter = 0;
    dupCounter = 0;
    remoteAddr = socketAddr;

    sprintf(fileName, "%s", &buff[2]);

    if (modeOctet(buff))
        file = fopen(fileName, "wb");
    else
        file = fopen(fileName, "w");

    if (file == NULL)
    {
        int err = errno;
        printf("Could not open file to write, error: %d\n", err);
        sendError("Could not open file to write.\n");
        state = LISTENING;
        remoteAddr.set_ip_address("");
    }
    else
    {
        // file ready for writing
        blockCounter = 0;
        state = WRITING;
        DEBUG_TFTP("Listening: Incoming file %s on TFTP connection from %s clientPort %d\r\n",
            fileName,
            remoteAddr.get_ip_address(),
            remoteAddr.get_port()
        );
    }
}

/**
 * @brief   Gets DATA block from file on disk into memory.
 * @note
 * @param
 * @retval
 */
void TFTPServer::getBlock()
{
    blockCounter++;

    blockBuff[0] = 0x00;
    blockBuff[1] = 0x03;
    blockBuff[2] = blockCounter >> 8;
    blockBuff[3] = blockCounter & 255;
    blockSize = 4 + fread((void*) &blockBuff[4], 1, 512, file);
}

/**
 * @brief   Sends DATA block to remote client.
 * @note
 * @param
 * @retval
 */
void TFTPServer::sendBlock()
{
    socket->sendto(socketAddr, blockBuff, blockSize);
}

/**
 * @brief   Compares host's IP and Port with connected remote machine.
 * @note
 * @param
 * @retval
 */
int TFTPServer::cmpHost()
{
    // char    ip[17];
    // strcpy(ip, socketAddr.get_ip_address());

    // int port = socketAddr.get_port();
    // return((strcmp(ip, remoteIP) == 0) && (port == remotePort));
    return (remoteAddr == socketAddr);
}

/**
 * @brief   Sends ACK to remote client.
 * @note
 * @param
 * @retval
 */
void TFTPServer::ack(int val)
{
    char    ack[4];
    ack[0] = 0x00;
    ack[1] = 0x04;
    if ((val > 603135) || (val < 0))
        val = 0;
    ack[2] = val >> 8;
    ack[3] = val & 255;
    socket->sendto(socketAddr, ack, 4);
}

/**
 * @brief   Sends ERROR message to remote client.
 * @note    msg A C-style string with error message to be sent.
 * @param
 * @retval
 */
void TFTPServer::sendError(const char* msg)
{
    errorBuff[0] = 0x00;
    errorBuff[1] = 0x05;
    errorBuff[2] = 0x00;
    errorBuff[3] = 0x00;
    errorBuff[4] = '\0';    // termination char
    strcat(&errorBuff[4], msg);

    int len = 4 + strlen(&errorBuff[4]) + 1;
    socket->sendto(socketAddr, errorBuff, len);
    DEBUG_TFTP("Error: %s\r\n", msg);
}

/**
 * @brief   Checks if connection mode of client is octet/binary.
 * @note    buff  A char array.
 * @param
 * @retval
 */
int TFTPServer::modeOctet(char* buff)
{
    int x = 2;

    while (buff[x++] != 0);
    // get beginning of mode field
    int y = x;
    while (buff[y] != 0)
    {
        buff[y] = tolower(buff[y]);
        y++;
    }   // make mode field lowercase

    return(strcmp(&buff[x++], "octet") == 0);
}
