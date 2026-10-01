/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2024, Laboratory of Plasma Physics- CNRS
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the “Software”), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
-- of the Software, and to permit persons to whom the Software is furnished to do
-- so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
-- INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
-- PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
-- HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
-- OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
-- SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#pragma once
#include <cdfpp/cdf-enums.hpp>
using namespace cdf;

#include <pybind11/native_enum.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;


template <typename T>
void def_enums_wrappers(T& mod)
{
    py::native_enum<cdf_majority>(mod, "Majority", "enum.Enum")
        .value("row", cdf_majority::row)
        .value("column", cdf_majority::column)
        .export_values()
        .finalize();

    py::native_enum<cdf_compression_type>(mod, "CompressionType", "enum.Enum")
        .value("no_compression", cdf_compression_type::no_compression)
        .value("gzip_compression", cdf_compression_type::gzip_compression)
        .value("rle_compression", cdf_compression_type::rle_compression)
        .value("ahuff_compression", cdf_compression_type::ahuff_compression)
        .value("huff_compression", cdf_compression_type::huff_compression)
#ifdef CDFPP_USE_ZSTD
        .value("zstd_compression", cdf_compression_type::zstd_compression)
#endif
#ifdef CDFPP_USE_BLOSC2
        .value("blosc2_compression", cdf_compression_type::blosc2_compression)
#endif
        .export_values()
        .finalize();

    // Not exported to the module: the names are too generic.
    py::native_enum<cdf_encoding>(mod, "Encoding", "enum.Enum")
        .value("network", cdf_encoding::network)
        .value("SUN", cdf_encoding::SUN)
        .value("VAX", cdf_encoding::VAX)
        .value("decstation", cdf_encoding::decstation)
        .value("SGi", cdf_encoding::SGi)
        .value("IBMPC", cdf_encoding::IBMPC)
        .value("IBMRS", cdf_encoding::IBMRS)
        .value("PPC", cdf_encoding::PPC)
        .value("HP", cdf_encoding::HP)
        .value("NeXT", cdf_encoding::NeXT)
        .value("ALPHAOSF1", cdf_encoding::ALPHAOSF1)
        .value("ALPHAVMSd", cdf_encoding::ALPHAVMSd)
        .value("ALPHAVMSg", cdf_encoding::ALPHAVMSg)
        .value("ALPHAVMSi", cdf_encoding::ALPHAVMSi)
        .value("ARM_LITTLE", cdf_encoding::ARM_LITTLE)
        .value("ARM_BIG", cdf_encoding::ARM_BIG)
        .value("IA64VMSi", cdf_encoding::IA64VMSi)
        .value("IA64VMSd", cdf_encoding::IA64VMSd)
        .value("IA64VMSg", cdf_encoding::IA64VMSg)
        .finalize();

    py::native_enum<cdf_sparse_records>(mod, "SparseRecords", "enum.Enum")
        .value("no_sparse_records", cdf_sparse_records::no_sparse_records)
        .value("pad_sparse_records", cdf_sparse_records::pad_sparse_records)
        .value("prev_sparse_records", cdf_sparse_records::prev_sparse_records)
        .export_values()
        .finalize();

    py::native_enum<CDF_Types>(mod, "DataType", "enum.Enum")
        .value("CDF_BYTE", CDF_Types::CDF_BYTE)
        .value("CDF_CHAR", CDF_Types::CDF_CHAR)
        .value("CDF_INT1", CDF_Types::CDF_INT1)
        .value("CDF_INT2", CDF_Types::CDF_INT2)
        .value("CDF_INT4", CDF_Types::CDF_INT4)
        .value("CDF_INT8", CDF_Types::CDF_INT8)
        .value("CDF_NONE", CDF_Types::CDF_NONE)
        .value("CDF_EPOCH", CDF_Types::CDF_EPOCH)
        .value("CDF_FLOAT", CDF_Types::CDF_FLOAT)
        .value("CDF_REAL4", CDF_Types::CDF_REAL4)
        .value("CDF_REAL8", CDF_Types::CDF_REAL8)
        .value("CDF_UCHAR", CDF_Types::CDF_UCHAR)
        .value("CDF_UINT1", CDF_Types::CDF_UINT1)
        .value("CDF_UINT2", CDF_Types::CDF_UINT2)
        .value("CDF_UINT4", CDF_Types::CDF_UINT4)
        .value("CDF_DOUBLE", CDF_Types::CDF_DOUBLE)
        .value("CDF_EPOCH16", CDF_Types::CDF_EPOCH16)
        .value("CDF_TIME_TT2000", CDF_Types::CDF_TIME_TT2000)
        .export_values()
        .finalize();
}
