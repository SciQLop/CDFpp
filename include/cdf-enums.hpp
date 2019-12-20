#pragma once
/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2019, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#include <cstdint>
#include <type_traits>

namespace cdf
{
enum class cdf_record_type : int32_t
{
    CDR = 1,
    GDR = 2,
    rVDR = 3,
    ADR = 4,
    AgrEDR = 5,
    VXR = 6,
    VVR = 7,
    zVDR = 8,
    AzEDR = 9,
    CCR = 10,
    CPR = 11,
    SPR = 12,
    CVVR = 13,
    UIR = -1,
};

enum class cdf_encoding : int32_t
{
    network = 1,
    SUN = 2,
    VAX = 3,
    decstation = 4,
    SGi = 5,
    IBMPC = 6,
    IBMRS = 7,
    PPC = 9,
    HP = 11,
    NeXT = 12,
    ALPHAOSF1 = 13,
    ALPHAVMSd = 14,
    ALPHAVMSg = 15,
    ALPHAVMSi = 16,
    ARM_LITTLE = 17,
    ARM_BIG = 18,
    IA64VMSi = 19,
    IA64VMSd = 20,
    IA64VMSg = 21
};

enum class CDF_Types : uint32_t
{
    CDF_INT1 = 1,
    CDF_INT2 = 2,
    CDF_INT4 = 4,
    CDF_INT8 = 8,
    CDF_UINT1 = 11,
    CDF_UINT2 = 12,
    CDF_UINT4 = 14,
    CDF_BYTE = 41,
    CDF_REAL4 = 21,
    CDF_REAL8 = 22,
    CDF_FLOAT = 44,
    CDF_DOUBLE = 45,
    CDF_EPOCH = 31,
    CDF_EPOCH16 = 32,
    CDF_TIME_TT2000 = 33,
    CDF_CHAR = 51,
    CDF_UCHAR = 52
};

struct tt2000_t
{
    int64_t value;
};

struct epoch
{
    double value;
};

template <CDF_Types type>
constexpr auto from_cdf_type()
{
    if constexpr (type == CDF_Types::CDF_CHAR)
        return char {};
    if constexpr (type == CDF_Types::CDF_BYTE)
        return int8_t {};
    if constexpr (type == CDF_Types::CDF_INT1)
        return int8_t {};
    if constexpr (type == CDF_Types::CDF_INT2)
        return int16_t {};
    if constexpr (type == CDF_Types::CDF_INT4)
        return int32_t {};
    if constexpr (type == CDF_Types::CDF_INT8)
        return int64_t {};
    if constexpr (type == CDF_Types::CDF_UINT1)
        return uint8_t {};
    if constexpr (type == CDF_Types::CDF_UINT2)
        return uint16_t {};
    if constexpr (type == CDF_Types::CDF_UINT4)
        return uint32_t {};
    if constexpr (type == CDF_Types::CDF_FLOAT)
        return float {};
    if constexpr (type == CDF_Types::CDF_DOUBLE)
        return double {};
}

std::size_t cdf_type_size(CDF_Types type)
{
    switch (type)
    {
        case CDF_Types::CDF_INT1:
        case CDF_Types::CDF_UINT1:
        case CDF_Types::CDF_BYTE:
        case CDF_Types::CDF_CHAR:
        case CDF_Types::CDF_UCHAR:
            return 1;
        case CDF_Types::CDF_INT2:
        case CDF_Types::CDF_UINT2:
            return 2;
        case CDF_Types::CDF_INT4:
        case CDF_Types::CDF_UINT4:
        case CDF_Types::CDF_FLOAT:
        case CDF_Types::CDF_REAL4:
            return 4;
        case CDF_Types::CDF_INT8:
        case CDF_Types::CDF_EPOCH:
        case CDF_Types::CDF_REAL8:
        case CDF_Types::CDF_DOUBLE:
        case CDF_Types::CDF_TIME_TT2000:
            return 8;
        case CDF_Types::CDF_EPOCH16:
            return 16;
    }
    return 0;
};


template <typename type>
constexpr CDF_Types to_cdf_type()
{
    if constexpr (std::is_same_v<type, float>)
        return CDF_Types::CDF_FLOAT;
    if constexpr (std::is_same_v<type, double>)
        return CDF_Types::CDF_DOUBLE;
    if constexpr (std::is_same_v<type, uint8_t>)
        return CDF_Types::CDF_UINT1;
    if constexpr (std::is_same_v<type, uint16_t>)
        return CDF_Types::CDF_UINT2;
    if constexpr (std::is_same_v<type, uint32_t>)
        return CDF_Types::CDF_UINT4;
    if constexpr (std::is_same_v<type, int8_t>)
        return CDF_Types::CDF_INT1;
    if constexpr (std::is_same_v<type, int16_t>)
        return CDF_Types::CDF_INT2;
    if constexpr (std::is_same_v<type, int32_t>)
        return CDF_Types::CDF_INT4;
    if constexpr (std::is_same_v<type, int64_t>)
        return CDF_Types::CDF_INT8;
    if constexpr (std::is_same_v<type, epoch>)
        return CDF_Types::CDF_EPOCH;
    if constexpr (std::is_same_v<type, tt2000_t>)
        return CDF_Types::CDF_TIME_TT2000;
}


template <CDF_Types type>
using from_cdf_type_t = decltype(from_cdf_type<type>());

}
