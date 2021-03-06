#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <iostream>
#include <assert.h>
#include <string.h>
#include <thread>

#include <vector>
#include <numeric>
#include <random>
#include <algorithm>

#include <chrono>

#include "liburing.h"

#define test_file "/mnt/fdb/toy/test_file"

#define QD 1000

#define SQ_POLL 0

struct time_info
{
	std::chrono::_V2::system_clock::time_point start;
	std::chrono::_V2::system_clock::time_point end;
};

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

int get_completion_time(struct io_uring *ring)
{
	struct io_uring_cqe *cqe;
	int ret = io_uring_wait_cqe(ring, &cqe);

	if (ret < 0)
	{
		perror("io_uring_wait_cqe");
		return ret;
	}

	if (cqe->res < 0)
		return cqe->res;

	time_info *tf = (time_info *)io_uring_cqe_get_data(cqe);

	tf->end = std::chrono::high_resolution_clock::now();

	io_uring_cqe_seen(ring, cqe);

	return std::chrono::duration_cast<std::chrono::nanoseconds>(tf->end - tf->start).count();
}

int main(int argc, char *argv[])
{
	struct io_uring ring;
	int ret;

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

		// Init ring.
#ifdef SQ_POLL

	struct io_uring_params params;

	memset(&params, 0, sizeof(params));
	params.flags |= IORING_SETUP_SQPOLL;
	params.sq_thread_idle = 2000;

	ret = io_uring_queue_init_params(QD, &ring, &params);

#else
	ret = io_uring_queue_init(QD, &ring, 0);

#endif

	if (ret < 0)
	{
		fprintf(stderr, "queue_init: %d\n", ret);
		return -1;
	}

	// Register fixed file
#ifdef SQ_POLL
	ret = io_uring_register_files(&ring, &fd, 1);

	if (ret)
	{
		fprintf(stderr, "Error registering buffers: %s", strerror(-ret));
		return 1;
	}
#endif

	// Random write offsets
	std::vector<off_t> offset(1000);
	auto engine = std::default_random_engine{};
	std::uniform_int_distribution<off_t> dist{0, file_size / 4096};
	std::generate(offset.begin(), offset.end(), [&dist, &engine]()
				  { return dist(engine); });

	// Create iovec buffer
	char *buffer = new char[4096];
	iovec *iov = new iovec;
	iov->iov_base = buffer;
	iov->iov_len = 4096;

	// Get cqe
	std::thread t([&ring]()
				  {
		printf("Getting cqe");
		for (int i = 0; i < 1000; ++i)
		{
			printf("%d %d ns\n", i, get_completion_time(&ring));
		} });

	// Submit sqe
	struct io_uring_sqe *sqe;
	for (auto i : offset)
	{
		sqe = io_uring_get_sqe(&ring);

		if (!sqe)
		{
			fprintf(stderr, "Could not get SQE.\n");
			return 1;
		}

#ifdef SQ_POLL
		io_uring_prep_writev(sqe, 0, iov, 1, i * 4096);
		sqe->flags |= IOSQE_FIXED_FILE;
#else
		io_uring_prep_writev(sqe, fd, iov, 1, i * 4096);
#endif
		time_info *tf = new time_info;

		tf->start = std::chrono::high_resolution_clock::now();

		io_uring_sqe_set_data(sqe, tf);
		io_uring_submit(&ring);
	}

	t.join();

	delete buffer;
	delete iov;

	// Tear down ring
	io_uring_queue_exit(&ring);

	return ret;
}
