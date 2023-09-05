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
#include <cdfpp/attribute.hpp>
#include <cdfpp/cdf-file.hpp>
#include <cdfpp/cdf-io/loading/loading.hpp>
#include <cdfpp/cdf-io/saving/saving.hpp>
#include <cdfpp/no_init_vector.hpp>
#include <cdfpp_config.h>

#include "attribute.hpp"
#include "repr.hpp"
#include "variable.hpp"


using namespace cdf;

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;
namespace docstrings
{
constexpr auto _CDF = R"delimiter(
A CDF file object.

Attributes
----------
attributes: dict
    file attributes
variables: dict
    file variables
majority: cdf_majority
    file majority
distribution_version: int
    file distribution version
lazy_loaded: bool
    file lazy loading state
compression: CompressionType
    file compression type

Methods
-------
add_attribute
    Adds an attribute to the file. Raises an exception if the attribute already exists.
add_variable
    Adds a variable to the file. Raises an exception if the variable already exists.

Examples
--------

.. code-block:: python

    >>> from pycdfpp import CDF, DataType, CompressionType
    >>> import numpy as np
    >>> cdf = CDF()
    >>> cdf.add_variable("var1", np.arange(10, dtype=np.int32), DataType.CDF_INT4, compression=CompressionType.gzip_compression)
    var1:
      shape: [ 10 ]
      type: CDF_INT1
      record varry: True
      compression: GNU GZIP

      Attributes:

    >>> cdf.add_attribute("attr1", ["value1"])
    attr1: "value1"
    >>> cdf.add_attribute("attr2", [[2]])
    attr2: [ [ [ 2 ] ] ]
    >>> cdf.add_attribute("attr3", [[1,2,3],[1.,2.,3.,],"hello world"])
    attr3: [ [ [ 1, 2, 3 ], [ 1, 2, 3 ], "hello world" ] ]

.. code-block:: python

    >>> from pycdfpp import CDF, DataType, CompressionType, load
    >>> import numpy as np
    >>> import requests
    >>> cdf = load(requests.get("https://spdf.gsfc.nasa.gov/pub/data/ace/mag/level_2_cdaweb/mfi_h0/2010/ac_h0_mfi_20100102_v05.cdf").content)
    >>> cdf
    CDF:
      version: 2.5.22
      majority: Adaptative column
      compression: None

    Attributes:
      TITLE: "ACE> Magnetometer Parameters"
      Project: [ [ "ACE>Advanced Composition Explorer", "ISTP>International Solar-Terrestrial Physics" ] ]
      Discipline: "Space Physics>Interplanetary Studies"
      Source_name: "AC>Advanced Composition Explorer"
      Data_type: "H0>16-Sec Level 2 Data"
      Descriptor: "MAG>ACE Magnetic Field Instrument"
      Data_version: "5"
      Generated_by: "ACE Science Center"
      Generation_date: "20100510"
      LINK_TEXT: "Release notes and other info available at"
      LINK_TITLE: "the ACE Science Center Level 2 Data website"
      HTTP_LINK: "http://www.srl.caltech.edu/ACE/ASC/level2/index.html"
      TEXT: [ [ "MAG - ACE Magnetic Field Experiment", "References: http://www.srl.caltech.edu/ACE/", "The quality of ACE level 2 data is such that it is suitable for serious ", "scientific study.  However, to avoid confusion and misunderstanding, it ", "is recommended that users consult with the appropriate ACE team members", "before publishing work derived from the data. The ACE team has worked ", "hard to ensure that the level 2 data are free from errors, but the team ", "cannot accept responsibility for erroneous data, or for misunderstandings ", "about how the data may be used. This is especially true if the appropriate ", "ACE team members are not consulted before publication. At the very ", "least, preprints should be forwarded to the ACE team before publication." ] ]
      MODS: [ [ "Initial Release 9/7/01 ", "12/04/02: Fixed description of Epoch time variable." ] ]
      ADID_ref: "NSSD0327"
      Logical_file_id: "AC_H0_MAG_20100102_V05"
      Logical_source: "AC_H0_MFI"
      Logical_source_description: "H0 - ACE Magnetic Field 16-Second Level 2 Data"
      PI_name: "N. Ness"
      PI_affiliation: "Bartol Research Institute"
      Mission_group: "ACE"
      Instrument_type: "Magnetic Fields (space)"
      Time_resolution: "16 second"
      Web_site: "http://www.srl.caltech.edu/ACE/"
      Acknowledgement: [ [ "Please acknowledge the Principal ", "Investigator, N. Ness of Bartol Research ", "Institute" ] ]
      Rules_of_use: [ [ "See the rules of use available from the ACE ", "Science Center at: ", "http://www.srl.caltech.edu/ACE/ASC/level2/policy_lvl2.html" ] ]

    Variables:
      Epoch: [ 5400, 1 ], [CDF_EPOCH], record varry:True, compression: None
      Time_PB5: [ 0, 3 ], [CDF_INT1], record varry:True, compression: None
      Magnitude: [ 5400, 1 ], [CDF_REAL4], record varry:True, compression: None
      BGSEc: [ 5400, 3 ], [CDF_REAL4], record varry:True, compression: None
      label_BGSE: [ 3, 6 ], [CDF_CHAR], record varry:Flase, compression: None
      BGSM: [ 5400, 3 ], [CDF_REAL4], record varry:True, compression: None
      label_bgsm: [ 3, 8 ], [CDF_CHAR], record varry:Flase, compression: None
      dBrms: [ 5400, 1 ], [CDF_REAL4], record varry:True, compression: None
      Q_FLAG: [ 5400, 1 ], [CDF_INT1], record varry:True, compression: None
      SC_pos_GSE: [ 5400, 3 ], [CDF_REAL4], record varry:True, compression: None
      label_pos_GSE: [ 3, 10 ], [CDF_CHAR], record varry:Flase, compression: None
      SC_pos_GSM: [ 5400, 3 ], [CDF_REAL4], record varry:True, compression: None
      label_pos_GSM: [ 3, 10 ], [CDF_CHAR], record varry:Flase, compression: None
      unit_time: [ 3, 4 ], [CDF_CHAR], record varry:Flase, compression: None
      label_time: [ 3, 27 ], [CDF_CHAR], record varry:Flase, compression: None
      format_time: [ 3, 2 ], [CDF_CHAR], record varry:Flase, compression: None
      cartesian: [ 3, 11 ], [CDF_CHAR], record varry:Flase, compression: None

    >>> cdf["Magnitude"]
    Magnitude:
      shape: [ 5400, 1 ]
      type: CDF_REAL4
      record varry: True
      compression: None
      Attributes:
        FIELDNAM: "B-field magnitude"
        VALIDMIN: [ [ [ 0 ] ] ]
        VALIDMAX: [ [ [ 500 ] ] ]
        SCALEMIN: [ [ [ 0 ] ] ]
        SCALEMAX: [ [ [ 10 ] ] ]
        UNITS: "nT"
        FORMAT: "F8.3"
        VAR_TYPE: "data"
        DICT_KEY: "magnetic_field>magnitude"
        FILLVAL: [ [ [ -1e+31 ] ] ]
        DEPEND_0: "Epoch"
        CATDESC: "B-field magnitude"
        LABLAXIS: "<|B|>"
        AVG_TYPE: " "
        DISPLAY_TYPE: "time_series"
        VAR_NOTES: " "
    >>> cdf["Magnitude"].values
    array([[7.715],
           [7.565],
           [7.668],
           ...,
           [5.909],
           [5.967],
           [5.926]], dtype=float32)


)delimiter";

}

template <typename T>
void def_cdf_wrapper(T& mod)
{
    py::class_<CDF>(mod, "CDF", docstrings::_CDF)
        .def(py::init<>())
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readonly("attributes", &CDF::attributes, py::return_value_policy::reference,
            py::keep_alive<0, 1>())
        .def_property_readonly("majority", [](const CDF& cdf) { return cdf.majority; })
        .def_property_readonly(
            "distribution_version", [](const CDF& cdf) { return cdf.distribution_version; })
        .def_property_readonly("lazy_loaded", [](const CDF& cdf) { return cdf.lazy_loaded; })
        .def_property(
            "compression", [](const CDF& cdf) { return cdf.compression; },
            [](CDF& cdf, cdf_compression_type ct) { cdf.compression = ct; })
        .def("__repr__", __repr__<CDF>)
        .def(
            "__getitem__", [](CDF& cd, const std::string& key) -> Variable& { return cd[key]; },
            py::return_value_policy::reference_internal)
        .def("__contains__",
            [](const CDF& cd, std::string& key) { return cd.variables.count(key) > 0; })
        .def(
            "__iter__",
            [](const CDF& cd)
            { return py::make_key_iterator(std::begin(cd.variables), std::end(cd.variables)); },
            py::keep_alive<0, 1>())
        .def(
            "items",
            [](const CDF& cd)
            { return py::make_iterator(std::begin(cd.variables), std::end(cd.variables)); },
            py::keep_alive<0, 1>())
        .def("__len__", [](const CDF& cd) { return std::size(cd.variables); })
        .def(
            "_add_variable",
            [](CDF& cdf, const std::string& name, bool is_nrv,
                cdf_compression_type compression) -> Variable&
            {
                if (cdf.variables.count(name) == 0)
                {
                    cdf.variables.emplace(name, name, std::size(cdf.variables), data_t {},
                        typename Variable::shape_t {}, cdf_majority::row, is_nrv, compression);
                    auto& var = cdf[name];
                    return var;
                }
                else
                {
                    throw std::invalid_argument { "Variable already exists" };
                }
            },
            py::arg("name"), py::arg("is_nrv") = false,
            py::arg("compression") = cdf_compression_type::no_compression,
            py::return_value_policy::reference_internal)
        .def(
            "_add_variable",
            [](CDF& cdf, const std::string& name, const py::buffer& buffer, CDF_Types data_type,
                bool is_nrv, cdf_compression_type compression) -> Variable&
            {
                if (cdf.variables.count(name) == 0)
                {
                    cdf.variables.emplace(name, name, std::size(cdf.variables), data_t {},
                        typename Variable::shape_t {}, cdf_majority::row, is_nrv, compression);
                    auto& var = cdf[name];
                    set_values(var, buffer, data_type);
                    return var;
                }
                else
                {
                    throw std::invalid_argument { "Variable already exists" };
                }
            },
            py::arg("name"), py::arg("values").noconvert(), py::arg("data_type"),
            py::arg("is_nrv") = false,
            py::arg("compression") = cdf_compression_type::no_compression,
            py::return_value_policy::reference_internal)
        .def("add_attribute",
            static_cast<Attribute& (*)(CDF&, const std::string&, std::vector<py_cdf_attr_data_t>&)>(
                add_attribute),
            docstrings::_CDFAddAttribute, py::arg { "name" }, py::arg { "values" },
            py::return_value_policy::reference_internal);
}

template <typename T>
void def_cdf_loading_functions(T& mod)
{
    mod.def(
        "load",
        [](py::bytes& buffer, bool iso_8859_1_to_utf8)
        {
            py::buffer_info info(py::buffer(buffer).request());
            return io::load(static_cast<char*>(info.ptr), static_cast<std::size_t>(info.size),
                iso_8859_1_to_utf8);
        },
        py::arg("buffer"), py::arg("iso_8859_1_to_utf8") = false, py::return_value_policy::move);

    mod.def(
        "lazy_load",
        [](py::buffer& buffer, bool iso_8859_1_to_utf8)
        {
            py::buffer_info info(buffer.request());
            if (info.ndim != 1)
                throw std::runtime_error("Incompatible buffer dimension!");
            return io::load(static_cast<char*>(info.ptr), info.shape[0], iso_8859_1_to_utf8, true);
        },
        py::arg("buffer"), py::arg("iso_8859_1_to_utf8") = false, py::return_value_policy::move,
        py::keep_alive<0, 1>());

    mod.def(
        "load",
        [](const char* fname, bool iso_8859_1_to_utf8, bool lazy_load)
        { return io::load(std::string { fname }, iso_8859_1_to_utf8, lazy_load); },
        py::arg("fname"), py::arg("iso_8859_1_to_utf8") = false, py::arg("lazy_load") = true,
        py::return_value_policy::move);
}

struct cdf_bytes
{
    no_init_vector<char> data;
};

template <typename T>
void def_cdf_saving_functions(T& mod)
{

    mod.def(
        "save",
        [](const CDF& cdf, const char* fname) { return io::save(cdf, std::string { fname }); },
        py::arg("cdf"), py::arg("fname"));


    py::class_<cdf_bytes>(mod, "_cdf_bytes", py::buffer_protocol())
        .def_buffer([](cdf_bytes& b) -> py::buffer_info
            { return py::buffer_info(b.data.data(), std::size(b.data), true); });

    mod.def(
        "save", [](const CDF& cdf) { return cdf_bytes { io::save(cdf) }; }, py::arg("cdf"));
}
