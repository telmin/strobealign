#ifndef IO_WRAP_HPP
#define IO_WRAP_HPP

#include <string>
#include <cstdint>

#include <zlib.h>
#include "igzip_lib.h"
#include <vector>

#include <thread>

class AbstructIO
{
public:
    AbstructIO(const std::string&)
    {}

    virtual ~AbstructIO() {}

    virtual int64_t read(void* buffer, size_t length) = 0;
    virtual std::string ReaderName() const = 0;

protected:
    virtual void open(const std::string& filename) = 0;
};


class GeneralIO : public AbstructIO
{
public:
    GeneralIO(const std::string& filename)
        : AbstructIO(filename)
    {
        open(filename);
    }

    virtual ~GeneralIO()
    {
        if(file) {
            gzclose(file);
        }
    }

    int64_t read(void* buffer, size_t length) override;
    std::string ReaderName() const override {
        return "GeneralIO";
    }

private:
    gzFile file;
    void open(const std::string& filename) override;
};

class IsalIO : public AbstructIO
{
public:
    IsalIO(const std::string& filename)
        : AbstructIO(filename),
          fd(-1),
          mmap_mem(nullptr),
          filesize(-1),
          mmap_size(-1),
          uncompressed_data(),
          uncompressed_data_work(),
          uncompressed_data_copied(0),
          compressed_data(nullptr),
          compressed_size(0),
          decompress_chunk_size(2567ull * 1024 * 1024),
          previous_member_size(1024ull * 1024 * 1024),
          thread_reader()
    {
        initialize();
        open(filename);
    }

    virtual ~IsalIO()
    {
        if(fd != -1) {
            close();
        }
    }

    int64_t read(void* buffer, size_t length) override;
    std::string ReaderName() const override {
        return "IsalIO";
    }

private:
    int fd;
    void* mmap_mem;
    ssize_t filesize;
    ssize_t mmap_size;
    std::vector<uint8_t> uncompressed_data;
    std::vector<uint8_t> uncompressed_data_work;
    size_t uncompressed_data_copied;

    uint8_t* compressed_data;
    size_t compressed_size;

    size_t decompress_chunk_size;
    size_t previous_member_size;

    std::thread thread_reader;

    inflate_state state;
    isal_gzip_header gz_hdr;

    void initialize();
    void open(const std::string& filename) override;
    void close();

    void decompress(size_t count);
};

#endif
