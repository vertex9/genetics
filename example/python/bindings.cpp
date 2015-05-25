// Copyright Andy Thomason 2015
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#pragma warning(disable : 4273)
#include <boost/genetics/augmented_string.hpp>
#include <boost/genetics/fasta.hpp>
#include <boost/python.hpp>

#include <iostream>
#include <fstream>

using namespace boost::python;
using namespace boost::genetics;

class Fasta {
public:
    Fasta() {
    }

    /// load from single FASTA file
    /*Fasta(const std::string &filename) {
        fasta = new fasta_file();
        fasta->append(filename);
        int num_indexed_chars = (56-lzcnt(fasta->size())/2;
        num_indexed_chars = std::max(6, std::min(12, num_indexed_chars));
        fasta->make_index((size_t)num_indexed_chars);
    }*/

    ~Fasta() {
        delete fasta;
    }

    /// load from FASTA files (takes several seconds)
    Fasta(const list &filenames, int num_indexed_chars)
    {
        size_t len = boost::python::len(filenames);
        fasta = new fasta_file();
        for (size_t i = 0; i != len; ++i) {
            fasta->append(boost::python::extract<std::string>(filenames[i]));
        }
        fasta->make_index((size_t)num_indexed_chars);
    }

    /// map instantly from a binary file
    void map(const std::string &binary_filename)
    {
        fasta = new mapped_fasta_file(binary_filename);
    }

    /// return a list of matches to 
    list find_inexact(const std::string &str, int max_distance, int max_results) const {
        std::vector<fasta_result> result;
        fasta->find_inexact(result, str, (size_t)max_distance, (size_t)max_results);
        list py_result;
        for (size_t i = 0; i != result.size(); ++i) {
            fasta_result &r = result[i];
            const chromosome &c = fasta->find_chromosome(r.location);
            py_result.append(boost::python::make_tuple(r.location, r.location - c.start, c.name));
        }

        return py_result;
    }

    list get_chromosomes() const {
        list result;

        return result;
    }

    /// write a binary file for use with a map
    void write_binary_file(const std::string &filename) const {
        writer sizer(nullptr, nullptr);
        fasta->write_binary(sizer);
        size_t size = (size_t)sizer.get_ptr();

        using namespace boost::interprocess;
        file_mapping fm(filename.c_str(), read_write);
        mapped_region region(fm, read_write, 0, size);
        char *p = (char*)region.get_address();
        char *end = p + region.get_size();
        writer w(p, end);
        fasta->write_binary(w);
    }

    /// write an ASCII file for human and legacy use.
    void write_ascii_file(const std::string &filename) const {
        std::ofstream os(filename);
        fasta->write_ascii(os);
    }

    fasta_file_interface *fasta;
}; 

BOOST_PYTHON_MODULE(genetics)
{
    // , (arg("filenames"), arg("num_indexed_chars")), "Create a reference file from several FASTA files, indexing to num_indexed_chars [3..16)"
    class_<Fasta>("Fasta", init<list, int>())
        .def("find_inexact", &Fasta::find_inexact, (arg("str"), arg("max_distance"), arg("max_results")), "find up to max_results hits with up to max_distance errors")
        .def("write_binary_file", &Fasta::write_binary_file, (arg("filename")), "write the reference to a binary file")
        .def("write_ascii_file", &Fasta::write_binary_file, (arg("filename")), "write the reference to a ascii file")
    ;
}

