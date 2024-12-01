/*
 * FILE          http_server.cpp
 *
 * AUTHORS
 *               Ilya Akkuzin <gr3yknigh1@gmail.com>
 *
 * NOTICE        (c) Copyright 2024 by Ilya Akkuzin. All rights reserved.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

/*
 * REFERENCE: https://learn.microsoft.com/en-us/windows/win32/winsock/creating-a-basic-winsock-application
 *
 * Quote from this reference:
 *
 * > The Iphlpapi.h header file is required if an application is using the IP Helper APIs. When the Iphlpapi.h header
 * > file is required, the #include line for the Winsock2.h header file should be placed before the #include line for
 * > the Iphlpapi.h header file.
 * >
 * > The Winsock2.h header file internally includes core elements from the Windows.h header file, so there is not
 * > usually an #include line for the Windows.h header file in Winsock applications. If an #include line is needed for
 * > the Windows.h header file, this should be preceded with the #define WIN32_LEAN_AND_MEAN macro. For historical
 * > reasons, the Windows.h header defaults to including the Winsock.h header file for Windows Sockets 1.1. The
 * > declarations in the Winsock.h header file will conflict with the declarations in the Winsock2.h header file
 * > required by Windows Sockets 2.0. The WIN32_LEAN_AND_MEAN macro prevents the Winsock.h from being included by the
 * > Windows.h header. An example illustrating this is shown below.
 *
 */
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#pragma comment(lib, "Ws2_32.lib")

#if !defined(ZERO_STRUCT)
#define ZERO_STRUCT(STRUCT_PTR) ZeroMemory(STRUCT_PTR, sizeof(*STRUCT_PTR))
#endif // !defined(ZERO_STRUCT)

constexpr size_t
WS__GetStringLength(const char *String) noexcept
{
    size_t Result = 0;

    while (String[Result] != 0) {
        Result++;
    }

    return Result;
}

struct WS__string8_view {
    const char *Data;
    size_t Length;

    constexpr WS__string8_view() noexcept : Data(nullptr), Length(0) {}
    constexpr WS__string8_view(const char *Data_) noexcept : Data(Data_), Length(WS__GetStringLength(Data_)) {}
    constexpr WS__string8_view(const char *Data_, size_t Length_) noexcept : Data(Data_), Length(Length_) {}
};

typedef WS__string8_view WS__http_version_t;
typedef char byte_t;

size_t
WS__GetPageSize(void) noexcept
{
    size_t pageSize;

    SYSTEM_INFO systemInfo = {0};
    GetSystemInfo(&systemInfo);

    pageSize = systemInfo.dwPageSize;

    return pageSize;
}

struct WS__write_buffer {
    byte_t *Data;
    byte_t *Cursor;
    size_t Capacity;
};

size_t
WS__WriteBuffer_GetWrittenBytesCount(WS__write_buffer *Buffer) noexcept
{
    return Buffer->Cursor - Buffer->Data;
}

WS__write_buffer
WS__WriteBuffer_Allocate(size_t Capacity) noexcept
{
    WS__write_buffer Result;

    Result.Data = static_cast<byte_t *>(malloc(Capacity));
    Result.Cursor = Result.Data;
    Result.Capacity = Capacity;

    return Result;
}

void
WS__WriteBuffer_Free(WS__write_buffer *Buffer) noexcept
{
    free(Buffer->Data);
    ZERO_STRUCT(Buffer);
}

void
WS__WriteBuffer_Zero(WS__write_buffer *Buffer) noexcept
{
    ZeroMemory(Buffer->Data, Buffer->Capacity);
}

void
WS__WriteBuffer_Write(WS__write_buffer *Buffer, char C) noexcept
{
    *Buffer->Cursor = C;
    Buffer->Cursor += 1;
}

void
WS__WriteBuffer_Write(WS__write_buffer *Buffer, const char *String) noexcept
{
    size_t StringLength = strlen(String);
    memcpy(Buffer->Cursor, String, StringLength);
    Buffer->Cursor += StringLength;
}

void
WS__WriteBuffer_Write(WS__write_buffer *Buffer, const void *Source, size_t Size) noexcept
{
    memcpy(Buffer->Cursor, Source, Size);
    Buffer->Cursor += Size;
}

void
WS__WriteBuffer_WriteFormat(WS__write_buffer *Buffer, const char *Format, ...) noexcept
{
    va_list VaArgs;
    va_start(VaArgs, Format);
    int Result = vsprintf(Buffer->Cursor, Format, VaArgs);
    va_end(VaArgs);

    assert(Result != -1);

    Buffer->Cursor += Result;
}

void
WS__ResetWriteBuffer(WS__write_buffer *Buffer) noexcept
{
    Buffer->Cursor = Buffer->Cursor;
}

static WS__http_version_t WS__HTTP_VERSION_1_1 = "HTTP/1.1";

enum class WS__http_status {
    Ok = 200,
};

static WS__string8_view WS__HTTP_STATUS_200_OK = "OK";

void
WS__WriteHttpStatus(WS__write_buffer *WriteBuffer, WS__http_status Status) noexcept
{
    const WS__string8_view *StatusToWrite = nullptr;

    switch (Status) {
    case WS__http_status::Ok:
        StatusToWrite = &WS__HTTP_STATUS_200_OK;
        break;
    default:
        break;
    }

    if (StatusToWrite == nullptr) {
        return;
    }

    WS__WriteBuffer_Write(WriteBuffer, StatusToWrite->Data, StatusToWrite->Length);
}

struct WS__http_response_header {
    WS__http_version_t Version;
    WS__http_status Status;
};

void
WS__WriteHttpResponseHeader(WS__write_buffer *WriteBuffer, const WS__http_response_header *ResponseHeader) noexcept
{
    assert(WriteBuffer);
    assert(ResponseHeader);

    //
    // Protocol version
    //
    WS__WriteBuffer_Write(WriteBuffer, ResponseHeader->Version.Data, ResponseHeader->Version.Length);
    WS__WriteBuffer_Write(WriteBuffer, ' ');

    //
    // Status
    //
    WS__WriteBuffer_WriteFormat(WriteBuffer, "%d ", ResponseHeader->Status);
    WS__WriteHttpStatus(WriteBuffer, ResponseHeader->Status);
    WS__WriteBuffer_Write(WriteBuffer, "\r\n\0\r\n\0\r\n");
}

int
main(int argc, char *argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    int Result = 0;
    WSADATA WSAData;
    ZERO_STRUCT(&WSAData);

    Result = WSAStartup(MAKEWORD(2, 2), &WSAData);
    assert(Result == 0);

    static const char *DefaultPort = "8080";

    struct addrinfo AddressHints; // TODO(gr3yknigh1): Research this struct [2024/12/01]
    ZERO_STRUCT(&AddressHints);
    AddressHints.ai_family = AF_INET;
    AddressHints.ai_socktype = SOCK_STREAM;
    AddressHints.ai_protocol = IPPROTO_TCP;
    AddressHints.ai_flags = AI_PASSIVE;

    struct addrinfo *AddressInfoResult = nullptr;

    Result = getaddrinfo(nullptr, DefaultPort, &AddressHints, &AddressInfoResult);

    if (Result != 0) {
        printf("E: getaddrinfo() failed! %d\n", Result);
        WSACleanup();
        return 1;
    }

    SOCKET ListenSocket = INVALID_SOCKET;

    ListenSocket = socket(AddressInfoResult->ai_family, AddressInfoResult->ai_socktype, AddressInfoResult->ai_protocol);

    if (ListenSocket == INVALID_SOCKET) {
        printf("E: socket() failed! %d\n", WSAGetLastError());
        freeaddrinfo(AddressInfoResult);
        WSACleanup();
        return 1;
    }

    Result = bind(ListenSocket, AddressInfoResult->ai_addr, static_cast<int>(AddressInfoResult->ai_addrlen));

    if (Result == SOCKET_ERROR) {
        printf("E: bind() failed! %d\n", Result);
        freeaddrinfo(AddressInfoResult);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    //
    // NOTE(gr3yknigh1): There is no need in `AddressInfoResult` no more, after `bind` call successully binded socket.
    // Reference: <https://learn.microsoft.com/en-us/windows/win32/winsock/binding-a-socket>
    //
    // > Once the bind function is called, the address information returned by the getaddrinfo function is no longer
    // > needed. The freeaddrinfo function is called to free the memory allocated by the getaddrinfo function for this
    // > address information.
    //
    // [2024/12/01]
    //

    freeaddrinfo(AddressInfoResult);

    //
    // Listening
    //

    Result = listen(ListenSocket, SOMAXCONN);

    if (Result == SOCKET_ERROR) {
        printf("E: listen() failed! %d\n", Result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    SOCKET ClientSocket = INVALID_SOCKET;

    puts("I: Waiting for connections...");

    ClientSocket = accept(ListenSocket, nullptr, nullptr);

    if (ClientSocket == INVALID_SOCKET) {
        printf("E: accept() failed! %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    puts("I: Accepted new connection!");

    static const size_t ReciveBufferCapacity = WS__GetPageSize();
    char *ReciveBuffer = static_cast<char *>(malloc(ReciveBufferCapacity));
    ZeroMemory(ReciveBuffer, ReciveBufferCapacity);
    assert(ReciveBuffer);

    WS__write_buffer SendWriteBuffer = WS__WriteBuffer_Allocate(WS__GetPageSize());
    WS__WriteBuffer_Zero(&SendWriteBuffer);

    //
    // Recive loop
    //

    int ReciveResult = 0;

    do {
        // NOTE(gr3yknigh1): `ReciveResult` == number of bytes recived [2024/12/01]
        ReciveResult = recv(ClientSocket, ReciveBuffer, static_cast<int>(ReciveBufferCapacity), 0);

        if (ReciveResult > 0) {
            printf("I: Bytes recevied: %d!\n", ReciveResult);

            //
            // Prepare response buffer
            //

            WS__http_response_header ResponseHeader;
            ResponseHeader.Version = WS__HTTP_VERSION_1_1;
            ResponseHeader.Status = WS__http_status::Ok;
            WS__WriteHttpResponseHeader(&SendWriteBuffer, &ResponseHeader);

            int SendResult =
                send(ClientSocket, SendWriteBuffer.Data, WS__WriteBuffer_GetWrittenBytesCount(&SendWriteBuffer), 0);
            if (SendResult == SOCKET_ERROR) {
                printf("E: send() failed! %d\n", WSAGetLastError());
                free(ReciveBuffer);
                WS__WriteBuffer_Free(&SendWriteBuffer);
                closesocket(ClientSocket);
                WSACleanup();
                return 1;
            }

            printf("I: Bytes sent: %d!\n", SendResult);
        } else if (ReciveResult == 0) {
            puts("I: Closing connection...");
        } else {
            printf("E: recv() failed! %d\n!", WSAGetLastError());
            free(ReciveBuffer);
            WS__WriteBuffer_Free(&SendWriteBuffer);
            closesocket(ClientSocket);
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }
    } while (ReciveResult > 0);

    closesocket(ClientSocket);

    //
    // Cleanup
    //
    puts("I: Cleanup...");
    free(ReciveBuffer);
    WS__WriteBuffer_Free(&SendWriteBuffer);
    closesocket(ListenSocket);
    WSACleanup();

    return 0;
}
