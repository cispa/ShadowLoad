// npy hack by Andreas Kogler

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

class npy_file {
    struct [[gnu::packed]] npy2_header {
        char     magic[6]   = { '\x93', 'N', 'U', 'M', 'P', 'Y' };
        char     major      = '\x02';
        char     minor      = '\x00';
        uint32_t header_len = 0;
    };

    npy2_header header_;
    std::string dtype_descr_;
    size_t      shape_offset_;

    uint64_t rows_ = 0;

    constexpr static inline char const *SHAPE_STR = "'shape': ( ";

    static std::string generate_npy_header(std::string const &metadata, std::vector<std::string> const &fields) {
        std::stringstream ss;

        ss << "{";
        {
            // we use a lot of space for the shape as we are constantly updating the shape
            ss << SHAPE_STR << "0                                                        ,   ), ";
            // we are placing the decr array of tuples
            ss << "'descr' : [";
            {
                // and add a zero width field to store the metadata in the title of the metadata field
                // this is not intended but fully supported with npy v2.0
                ss << "( ('''" << metadata << "''', 'metadata'), 'V0') ";

                // fill the remaining fields
                for ( auto &f : fields ) {
                    ss << ", " << f;
                }
            }
            ss << "], ";
            // additional stuff
            ss << "'fortran_order':False ";
        }
        ss << "}";

        // generate the descr and pad it to the required format length
        auto dtype_descr = ss.str();

        uint64_t cur_size = sizeof(npy2_header) + dtype_descr.size();
        uint64_t extend   = 64 - cur_size % 64;

        dtype_descr.resize(dtype_descr.size() + extend - 1, ' ');
        dtype_descr += '\n';

        return dtype_descr;
    }

  public:
    npy_file(std::string metadata, std::vector<std::string> fields) {

        // get the dtype specifier
        dtype_descr_ = generate_npy_header(metadata, fields);

        // add the length of the dtype specifier to the header
        header_.header_len = dtype_descr_.size();

        // find the offset of the shape field relative to the file start
        shape_offset_ = sizeof(npy2_header) + dtype_descr_.find(SHAPE_STR) + strlen(SHAPE_STR);
    }

    void write_header(FILE *file) {
        // header
        fwrite(&header_, sizeof(npy2_header), 1, file);
        // dtype specifier
        fwrite(dtype_descr_.data(), dtype_descr_.size(), 1, file);
    }

    void write_rows(FILE *file, uint8_t *data, size_t n, size_t rows) {
        // first write the new content
        fseek(file, 0, SEEK_END);
        fwrite(data, n, 1, file);

        // then update the shape specifier -> we always have a valid npy file
        rows_ += rows;
        fseek(file, shape_offset_, SEEK_SET);
        fprintf(file, "%ld", rows_);

        // flush the file to make sure the content is at least somewhere
        fflush(file);
    }
};
