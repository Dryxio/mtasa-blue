/*****************************************************************************
 *
 *  PROJECT:     MTA:SA Server - Android Network Module
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Server/net-android/CNetBitStreamAndroid.cpp
 *  PURPOSE:     BitStream implementation for Android network module
 *
 *****************************************************************************/

#include "CNetBitStreamAndroid.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdio>

//=============================================================================
// Static member definition for CRefCountable
// This creates the shared critical section used by all CRefCountable instances
//=============================================================================
CCriticalSection CRefCountable::ms_CS;

//=============================================================================
// Constructor / Destructor
//=============================================================================

CNetBitStreamAndroid::CNetBitStreamAndroid(const void* pData, unsigned int uiDataSize,
                                           unsigned short usVersion)
    : m_writeBitOffset(0)
    , m_readBitOffset(0)
    , m_version(usVersion)
{
    if (pData && uiDataSize > 0)
    {
        m_data.resize(uiDataSize);
        memcpy(m_data.data(), pData, uiDataSize);
        m_writeBitOffset = uiDataSize * 8;
    }
    else
    {
        m_data.reserve(256);  // Initial capacity
    }
}

CNetBitStreamAndroid::~CNetBitStreamAndroid()
{
}

//=============================================================================
// Offset Management
//=============================================================================

int CNetBitStreamAndroid::GetReadOffsetAsBits()
{
    printf("[bitstream] GetReadOffsetAsBits() -> %u\n", m_readBitOffset);
    fflush(stdout);
    return m_readBitOffset;
}

void CNetBitStreamAndroid::SetReadOffsetAsBits(int iOffset)
{
    m_readBitOffset = std::max(0, std::min(iOffset, (int)m_writeBitOffset));
}

void CNetBitStreamAndroid::Reset()
{
    m_data.clear();
    m_writeBitOffset = 0;
    m_readBitOffset = 0;
}

void CNetBitStreamAndroid::ResetReadPointer()
{
    m_readBitOffset = 0;
}

//=============================================================================
// Capacity Management
//=============================================================================

void CNetBitStreamAndroid::EnsureCapacity(unsigned int bitsNeeded)
{
    unsigned int bytesNeeded = (m_writeBitOffset + bitsNeeded + 7) / 8;
    if (bytesNeeded > m_data.size())
    {
        m_data.resize(bytesNeeded + 32);  // Add some extra
    }
}

//=============================================================================
// Basic Write Operations
//=============================================================================

void CNetBitStreamAndroid::Write(const unsigned char& input)
{
    WriteBits((const char*)&input, 8);
}

void CNetBitStreamAndroid::Write(const char& input)
{
    WriteBits(&input, 8);
}

void CNetBitStreamAndroid::Write(const unsigned short& input)
{
    WriteBits((const char*)&input, 16);
}

void CNetBitStreamAndroid::Write(const short& input)
{
    WriteBits((const char*)&input, 16);
}

void CNetBitStreamAndroid::Write(const unsigned int& input)
{
    WriteBits((const char*)&input, 32);
}

void CNetBitStreamAndroid::Write(const int& input)
{
    WriteBits((const char*)&input, 32);
}

void CNetBitStreamAndroid::Write(const float& input)
{
    WriteBits((const char*)&input, 32);
}

void CNetBitStreamAndroid::Write(const double& input)
{
    WriteBits((const char*)&input, 64);
}

void CNetBitStreamAndroid::Write(const char* input, int numberOfBytes)
{
    if (numberOfBytes > 0)
    {
        WriteBits(input, numberOfBytes * 8);
    }
}

void CNetBitStreamAndroid::Write(const ISyncStructure* syncStruct)
{
    // SyncStructure writes itself
    if (syncStruct)
        syncStruct->Write(*this);
}

//=============================================================================
// Compressed Write Operations
//=============================================================================

void CNetBitStreamAndroid::WriteCompressed(const unsigned char& input)
{
    if (input == 0)
    {
        WriteBit(false);
    }
    else
    {
        WriteBit(true);
        Write(input);
    }
}

void CNetBitStreamAndroid::WriteCompressed(const char& input)
{
    WriteCompressed((unsigned char)input);
}

void CNetBitStreamAndroid::WriteCompressed(const unsigned short& input)
{
    if ((input & 0xFF00) == 0)
    {
        WriteBit(true);
        Write((unsigned char)(input & 0xFF));
    }
    else
    {
        WriteBit(false);
        Write(input);
    }
}

void CNetBitStreamAndroid::WriteCompressed(const short& input)
{
    WriteCompressed((unsigned short)input);
}

void CNetBitStreamAndroid::WriteCompressed(const unsigned int& input)
{
    if ((input & 0xFFFF0000) == 0)
    {
        WriteBit(true);
        WriteCompressed((unsigned short)(input & 0xFFFF));
    }
    else
    {
        WriteBit(false);
        Write(input);
    }
}

void CNetBitStreamAndroid::WriteCompressed(const int& input)
{
    WriteCompressed((unsigned int)input);
}

void CNetBitStreamAndroid::WriteCompressed(const float& input)
{
    Write(input);  // No compression for floats
}

void CNetBitStreamAndroid::WriteCompressed(const double& input)
{
    Write(input);  // No compression for doubles
}

//=============================================================================
// Bit Operations
//=============================================================================

void CNetBitStreamAndroid::WriteBits(const char* input, unsigned int numbits)
{
    if (numbits == 0)
        return;

    EnsureCapacity(numbits);

    // Fast path: byte-aligned write
    if ((m_writeBitOffset % 8) == 0)
    {
        unsigned int startByte = m_writeBitOffset / 8;
        unsigned int fullBytes = numbits / 8;
        unsigned int remainingBits = numbits % 8;

        if (fullBytes > 0)
        {
            memcpy(m_data.data() + startByte, input, fullBytes);
            m_writeBitOffset += fullBytes * 8;
        }

        if (remainingBits > 0)
        {
            unsigned char mask = (1 << remainingBits) - 1;
            m_data[m_writeBitOffset / 8] = input[fullBytes] & mask;
            m_writeBitOffset += remainingBits;
        }
    }
    else
    {
        // Slow path: bit-by-bit write
        for (unsigned int i = 0; i < numbits; i++)
        {
            bool bit = (input[i / 8] >> (i % 8)) & 1;
            WriteBit(bit);
        }
    }
}

void CNetBitStreamAndroid::WriteBit(bool input)
{
    EnsureCapacity(1);

    unsigned int byteOffset = m_writeBitOffset / 8;
    unsigned int bitOffset = m_writeBitOffset % 8;

    if (bitOffset == 0)
    {
        m_data[byteOffset] = 0;
    }

    if (input)
    {
        m_data[byteOffset] |= (1 << bitOffset);
    }

    m_writeBitOffset++;
}

void CNetBitStreamAndroid::WriteNormVector(float x, float y, float z)
{
    // Simplified - write as compressed floats
    Write(x);
    Write(y);
    Write(z);
}

void CNetBitStreamAndroid::WriteVector(float x, float y, float z)
{
    Write(x);
    Write(y);
    Write(z);
}

void CNetBitStreamAndroid::WriteNormQuat(float w, float x, float y, float z)
{
    Write(w);
    Write(x);
    Write(y);
    Write(z);
}

void CNetBitStreamAndroid::WriteOrthMatrix(float m00, float m01, float m02,
                                            float m10, float m11, float m12,
                                            float m20, float m21, float m22)
{
    Write(m00); Write(m01); Write(m02);
    Write(m10); Write(m11); Write(m12);
    Write(m20); Write(m21); Write(m22);
}

//=============================================================================
// Basic Read Operations
//=============================================================================

bool CNetBitStreamAndroid::Read(unsigned char& output)
{
    printf("[bitstream] Read(uchar&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    bool result = ReadBits((char*)&output, 8);
    printf("[bitstream]   -> value=%u (0x%02X), result=%s\n", output, output, result ? "true" : "false");
    fflush(stdout);
    return result;
}

bool CNetBitStreamAndroid::Read(char& output)
{
    printf("[bitstream] Read(char&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    bool result = ReadBits(&output, 8);
    printf("[bitstream]   -> value=%d (0x%02X), result=%s\n", output, (unsigned char)output, result ? "true" : "false");
    fflush(stdout);
    return result;
}

bool CNetBitStreamAndroid::Read(unsigned short& output)
{
    printf("[bitstream] Read(ushort&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    bool result = ReadBits((char*)&output, 16);
    printf("[bitstream]   -> value=%u (0x%04X), result=%s\n", output, output, result ? "true" : "false");
    fflush(stdout);
    return result;
}

bool CNetBitStreamAndroid::Read(short& output)
{
    printf("[bitstream] Read(short&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    bool result = ReadBits((char*)&output, 16);
    printf("[bitstream]   -> value=%d (0x%04X), result=%s\n", output, (unsigned short)output, result ? "true" : "false");
    fflush(stdout);
    return result;
}

bool CNetBitStreamAndroid::Read(unsigned int& output)
{
    printf("[bitstream] Read(uint&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    bool result = ReadBits((char*)&output, 32);
    printf("[bitstream]   -> value=%u (0x%08X), result=%s\n", output, output, result ? "true" : "false");
    fflush(stdout);
    return result;
}

bool CNetBitStreamAndroid::Read(int& output)
{
    printf("[bitstream] Read(int&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    bool result = ReadBits((char*)&output, 32);
    printf("[bitstream]   -> value=%d (0x%08X), result=%s\n", output, (unsigned int)output, result ? "true" : "false");
    fflush(stdout);
    return result;
}

bool CNetBitStreamAndroid::Read(float& output)
{
    printf("[bitstream] Read(float&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    bool result = ReadBits((char*)&output, 32);
    printf("[bitstream]   -> value=%f, result=%s\n", output, result ? "true" : "false");
    fflush(stdout);
    return result;
}

bool CNetBitStreamAndroid::Read(double& output)
{
    printf("[bitstream] Read(double&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    bool result = ReadBits((char*)&output, 64);
    printf("[bitstream]   -> value=%f, result=%s\n", output, result ? "true" : "false");
    fflush(stdout);
    return result;
}

bool CNetBitStreamAndroid::Read(char* output, int numberOfBytes)
{
    printf("[bitstream] Read(void*, %d bytes) at bit %u\n", numberOfBytes, m_readBitOffset);
    fflush(stdout);
    if (numberOfBytes <= 0)
    {
        printf("[bitstream]   -> (empty read)\n");
        fflush(stdout);
        return true;
    }
    bool result = ReadBits(output, numberOfBytes * 8);
    printf("[bitstream]   -> result=%s, first bytes: ", result ? "true" : "false");
    for (int i = 0; i < std::min(numberOfBytes, 16); i++)
        printf("%02X ", (unsigned char)output[i]);
    printf("\n");
    fflush(stdout);
    return result;
}

bool CNetBitStreamAndroid::Read(ISyncStructure* syncStruct)
{
    printf("[bitstream] Read(ISyncStructure*) at bit %u, ptr=%p\n", m_readBitOffset, (void*)syncStruct);
    fflush(stdout);
    if (!syncStruct)
        return false;
    return syncStruct->Read(*this);
}

//=============================================================================
// Compressed Read Operations
//=============================================================================

bool CNetBitStreamAndroid::ReadCompressed(unsigned char& output)
{
    printf("[bitstream] ReadCompressed(uchar&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    if (ReadBit())
    {
        bool result = Read(output);
        printf("[bitstream]   -> (non-zero) value=%u, result=%s\n", output, result ? "true" : "false");
        fflush(stdout);
        return result;
    }
    else
    {
        output = 0;
        printf("[bitstream]   -> (zero) value=0\n");
        fflush(stdout);
        return true;
    }
}

bool CNetBitStreamAndroid::ReadCompressed(char& output)
{
    printf("[bitstream] ReadCompressed(char&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    return ReadCompressed((unsigned char&)output);
}

bool CNetBitStreamAndroid::ReadCompressed(unsigned short& output)
{
    printf("[bitstream] ReadCompressed(ushort&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    if (ReadBit())
    {
        unsigned char low;
        if (!Read(low))
            return false;
        output = low;
        printf("[bitstream]   -> (compressed) value=%u\n", output);
        fflush(stdout);
        return true;
    }
    else
    {
        bool result = Read(output);
        printf("[bitstream]   -> (full) value=%u, result=%s\n", output, result ? "true" : "false");
        fflush(stdout);
        return result;
    }
}

bool CNetBitStreamAndroid::ReadCompressed(short& output)
{
    printf("[bitstream] ReadCompressed(short&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    return ReadCompressed((unsigned short&)output);
}

bool CNetBitStreamAndroid::ReadCompressed(unsigned int& output)
{
    printf("[bitstream] ReadCompressed(uint&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    if (ReadBit())
    {
        unsigned short low;
        if (!ReadCompressed(low))
            return false;
        output = low;
        printf("[bitstream]   -> (compressed) value=%u\n", output);
        fflush(stdout);
        return true;
    }
    else
    {
        bool result = Read(output);
        printf("[bitstream]   -> (full) value=%u, result=%s\n", output, result ? "true" : "false");
        fflush(stdout);
        return result;
    }
}

bool CNetBitStreamAndroid::ReadCompressed(int& output)
{
    printf("[bitstream] ReadCompressed(int&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    return ReadCompressed((unsigned int&)output);
}

bool CNetBitStreamAndroid::ReadCompressed(float& output)
{
    printf("[bitstream] ReadCompressed(float&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    return Read(output);
}

bool CNetBitStreamAndroid::ReadCompressed(double& output)
{
    printf("[bitstream] ReadCompressed(double&) at bit %u\n", m_readBitOffset);
    fflush(stdout);
    return Read(output);
}

//=============================================================================
// Bit Read Operations
//=============================================================================

bool CNetBitStreamAndroid::ReadBits(char* output, unsigned int numbits)
{
    printf("[bitstream] ReadBits(%u bits) at bit %u (total=%u)\n", numbits, m_readBitOffset, m_writeBitOffset);
    fflush(stdout);

    if (numbits == 0)
    {
        printf("[bitstream]   -> (zero bits requested)\n");
        fflush(stdout);
        return true;
    }

    if (m_readBitOffset + numbits > m_writeBitOffset)
    {
        printf("[bitstream]   -> FAILED: not enough bits (need %u, have %u)\n",
               numbits, m_writeBitOffset - m_readBitOffset);
        fflush(stdout);
        return false;
    }

    // Fast path: byte-aligned read
    if ((m_readBitOffset % 8) == 0)
    {
        unsigned int startByte = m_readBitOffset / 8;
        unsigned int fullBytes = numbits / 8;
        unsigned int remainingBits = numbits % 8;

        if (fullBytes > 0)
        {
            memcpy(output, m_data.data() + startByte, fullBytes);
            m_readBitOffset += fullBytes * 8;
        }

        if (remainingBits > 0)
        {
            unsigned char mask = (1 << remainingBits) - 1;
            output[fullBytes] = m_data[m_readBitOffset / 8] & mask;
            m_readBitOffset += remainingBits;
        }
    }
    else
    {
        // Slow path: bit-by-bit read
        memset(output, 0, (numbits + 7) / 8);
        for (unsigned int i = 0; i < numbits; i++)
        {
            if (ReadBit())
            {
                output[i / 8] |= (1 << (i % 8));
            }
        }
    }

    printf("[bitstream]   -> OK, new offset=%u\n", m_readBitOffset);
    fflush(stdout);
    return true;
}

bool CNetBitStreamAndroid::ReadBit()
{
    if (m_readBitOffset >= m_writeBitOffset)
    {
        printf("[bitstream] ReadBit() at bit %u -> FAILED (past end)\n", m_readBitOffset);
        fflush(stdout);
        return false;
    }

    unsigned int byteOffset = m_readBitOffset / 8;
    unsigned int bitOffset = m_readBitOffset % 8;

    m_readBitOffset++;

    bool result = (m_data[byteOffset] >> bitOffset) & 1;
    printf("[bitstream] ReadBit() at bit %u -> %d\n", m_readBitOffset - 1, result ? 1 : 0);
    fflush(stdout);
    return result;
}

bool CNetBitStreamAndroid::ReadNormVector(float& x, float& y, float& z)
{
    return Read(x) && Read(y) && Read(z);
}

bool CNetBitStreamAndroid::ReadVector(float& x, float& y, float& z)
{
    return Read(x) && Read(y) && Read(z);
}

bool CNetBitStreamAndroid::ReadNormQuat(float& w, float& x, float& y, float& z)
{
    return Read(w) && Read(x) && Read(y) && Read(z);
}

bool CNetBitStreamAndroid::ReadOrthMatrix(float& m00, float& m01, float& m02,
                                           float& m10, float& m11, float& m12,
                                           float& m20, float& m21, float& m22)
{
    return Read(m00) && Read(m01) && Read(m02) &&
           Read(m10) && Read(m11) && Read(m12) &&
           Read(m20) && Read(m21) && Read(m22);
}

//=============================================================================
// Size Information
//=============================================================================

int CNetBitStreamAndroid::GetNumberOfBitsUsed() const
{
    printf("[bitstream] GetNumberOfBitsUsed() -> %u\n", m_writeBitOffset);
    fflush(stdout);
    return m_writeBitOffset;
}

int CNetBitStreamAndroid::GetNumberOfBytesUsed() const
{
    return (m_writeBitOffset + 7) / 8;
}

int CNetBitStreamAndroid::GetNumberOfUnreadBits() const
{
    return m_writeBitOffset - m_readBitOffset;
}

//=============================================================================
// Alignment
//=============================================================================

void CNetBitStreamAndroid::AlignWriteToByteBoundary() const
{
    if (m_writeBitOffset % 8 != 0)
    {
        const_cast<CNetBitStreamAndroid*>(this)->m_writeBitOffset =
            ((m_writeBitOffset + 7) / 8) * 8;
    }
}

void CNetBitStreamAndroid::AlignReadToByteBoundary() const
{
    if (m_readBitOffset % 8 != 0)
    {
        const_cast<CNetBitStreamAndroid*>(this)->m_readBitOffset =
            ((m_readBitOffset + 7) / 8) * 8;
    }
}

//=============================================================================
// Data Access
//=============================================================================

unsigned char* CNetBitStreamAndroid::GetData() const
{
    return const_cast<unsigned char*>(m_data.data());
}

unsigned short CNetBitStreamAndroid::Version() const
{
    printf("[bitstream] Version() -> 0x%04X (%u)\n", m_version, m_version);
    fflush(stdout);
    return m_version;
}
