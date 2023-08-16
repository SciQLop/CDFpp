#pragma once
/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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
#pragma once


#include "cdfpp/no_init_vector.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <utility>

namespace cdf::io::buffers
{

struct vector_writer
{
    no_init_vector<char>& data;
    std::size_t global_offset;
    vector_writer(no_init_vector<char>& data) : data { data }, global_offset { 0 } { }

    std::size_t write(const char* const data_ptr, std::size_t count)
    {
        data.resize(global_offset + count);
        memcpy(data.data() + global_offset, data_ptr, count);
        global_offset += count;
        return global_offset;
    }

    std::size_t fill(const char v, std::size_t count)
    {
        data.resize(global_offset + count);
        memset(data.data() + global_offset, v, count);
        global_offset += count;
        return global_offset;
    }

    [[nodiscard]] std::size_t offset() const noexcept { return this->global_offset; }
};

struct file_writer
{
    std::fstream os;
    std::size_t global_offset;
    file_writer(const std::string& fname) : global_offset { 0 }
    {
        this->os
            = std::fstream(fname, std::fstream::out | std::fstream::binary | std::fstream::trunc);
    }
    ~file_writer()
    {
        if (is_open())
        {
            os.flush();
            os.close();
        }
    }

    [[nodiscard]] bool is_open() const noexcept { return os.is_open(); }

    std::size_t write(const char* const data_ptr, std::size_t count)
    {
        os.write(data_ptr, count);
        global_offset += count;
        return global_offset;
    }

    std::size_t fill(const char v, std::size_t count)
    {
        std::vector<char> values(count, v);
        os.write(values.data(), count);
        global_offset += count;
        return global_offset;
    }

    [[nodiscard]] std::size_t offset() const noexcept { return this->global_offset; }
};
}
