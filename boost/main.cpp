#include <iostream>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <vector>
#include <numeric>
#include <random>
#include <algorithm>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#define test_file "/mnt/fdb/toy/test_file"

static int get_file_size(int fd, off_t *size)
{
    struct stat st;

    if (fstat(fd, &st) < 0)
        return -1;
    if (S_ISREG(st.st_mode))
    {
        *size = st.st_size;
        return 0;
    }
    else if (S_ISBLK(st.st_mode))
    {
        unsigned long long bytes;

        if (ioctl(fd, BLKGETSIZE64, &bytes) != 0)
            return -1;

        *size = bytes;
        return 0;
    }
    return -1;
}

int main()
{
    boost::asio::io_service io;

    // Open file
    int fd = open(test_file, O_DIRECT | O_RDWR);
    if (fd < 0)
    {
        perror("open");
        return -1;
    }
    // Get file size
    off_t file_size;
    if (get_file_size(fd, &file_size))
        return -1;

    // Boost random access_file
    boost::asio::random_access_file file(io, fd);

    // Random write offsets
    std::vector<off_t> offset(1000);
    auto engine = std::default_random_engine{};
    std::uniform_int_distribution<off_t> dist{0, file_size / 4096};
    std::generate(offset.begin(), offset.end(), [&dist, &engine]()
                  { return dist(engine); });

    void *buffer;
    posix_memalign(&buffer, 4096, 4096);
    auto bb = boost::asio::const_buffers_1(buffer, 4096);

    for (int i = 0; i < 1000; ++i)
    {
        auto start = std::chrono::high_resolution_clock::now();

        file.async_write_some_at(offset.at(i) * 4096, bb,
                                 [&start, i](const boost::system::error_code &error, std::size_t bytes_transferred) {
                                     printf("%d %d ns\n", i, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()-start).count());
                                 });
    }

    io.run();

    return 0;
}