/*
Copyright (c) 2011, Mosalam Ebrahimi <m.ebrahimi@ieee.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>

#include "framegrabber.h"

#define CLEAR(x) memset (&(x), 0, sizeof (x))

using namespace std;

FrameGrabber::FrameGrabber(const string device_name, const int w, const int h,
						   const int fps, const bool freq_50hz,
						   const int num_buffers):
	fd(-1), failed(0)
{
	dev_name.assign(device_name);
	width = w;
	height = h;
	n_buffers = num_buffers;
	pixel_size = 2;
	frame_rate = fps;
	power_line_freq50 = freq_50hz;
}

FrameGrabber::~FrameGrabber() 
{
}

int FrameGrabber::Init()
{
	// open device
	struct stat st;

	if (-1 == stat (dev_name.c_str(), &st)) {
		fprintf (stderr, "Cannot identify '%s': %d, %s\n",
				dev_name.c_str(), errno, strerror (errno));
		return 0;
	}

	if (!S_ISCHR (st.st_mode)) {
		fprintf (stderr, "%s is no device\n", dev_name.c_str());
		return 0;
	}

	fd = open (dev_name.c_str(), O_RDWR /* required */ | O_NONBLOCK, 0);

	if (-1 == fd) {
		fprintf (stderr, "Cannot open '%s': %d, %s\n",
				dev_name.c_str(), errno, strerror (errno));
		return 0;
	}

	// init
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n",
					dev_name.c_str());
			return 0;
		} else {
			fprintf(stderr, "VIDIOC_QUERYCAP\n");
			return 0;
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n",
				dev_name.c_str());
		return 0;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n",
				dev_name.c_str());
		return 0;
	}

	// Select video input, video standard and tune here
	CLEAR (cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; // reset to default 
		
		if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
				case EINVAL:
					// Cropping not supported 
					break;
				default:
					// Errors ignored
					break;
			}
		}
	} else {	
		// Errors ignored
	}

	CLEAR(fmt);

	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = width; 
	fmt.fmt.pix.height      = height;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

	if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt)) {
		fprintf(stderr, "VIDIOC_S_FMT\n");
		return 0;
	}

	// Note VIDIOC_S_FMT may change width and height
	width = fmt.fmt.pix.width;
	height = fmt.fmt.pix.height;

	struct v4l2_streamparm setfps;
	memset (&setfps, 0, sizeof(struct v4l2_streamparm));
	setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	setfps.parm.capture.timeperframe.numerator = 1;
	setfps.parm.capture.timeperframe.denominator = frame_rate;
	xioctl (fd, VIDIOC_S_PARM, &setfps);

	struct v4l2_control ctrl;
	ctrl.id = V4L2_CID_POWER_LINE_FREQUENCY;
	ctrl.value = (power_line_freq50==true)?1:0;
	xioctl(fd, VIDIOC_S_CTRL, &ctrl);

	// Buggy driver paranoia
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	// init mmap
	struct v4l2_requestbuffers req;
	CLEAR(req);

	req.count    = n_buffers;
	req.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory   = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
					"memory mapping\n", dev_name.c_str());
			return 0;
		} else {
			fprintf(stderr, "VIDIOC_REQBUFS");
			return 0;
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n",
				dev_name.c_str());
		return 0;
	}

	// number of allocated buffers
	n_buffers = req.count;

	buffers = static_cast<buffer*>(calloc(req.count, sizeof (*buffers)));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		return 0;
	}

	for (unsigned int i_buffer = 0; i_buffer < req.count; ++i_buffer) {
		struct v4l2_buffer buf;
		
		CLEAR(buf);
		
		buf.type      = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory    = V4L2_MEMORY_MMAP;
		buf.index     = i_buffer;
		
		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) {
			fprintf(stderr, "VIDIOC_QUERYBUF");
			return 0;
		}
		
		buffers[i_buffer].length = buf.length;
		buffers[i_buffer].start =
		mmap(NULL /* start anywhere */,
			buf.length,
			PROT_READ | PROT_WRITE /* required */,
			MAP_SHARED /* recommended */,
			fd, buf.m.offset);
			
		if (MAP_FAILED == buffers[i_buffer].start) {
			fprintf(stderr, "VIDIOC_QUERYBUF");
			return 0;
		}
	}

	return 1;
}


int FrameGrabber::StartCapturing() 
{
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;
		
		CLEAR(buf);
		
		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;
		
		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf)) {
			fprintf(stderr, "VIDIOC_QBUF");
			return 0;
		}
	}
	
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	if (-1 == xioctl (fd, VIDIOC_STREAMON, &type)) {
		fprintf(stderr, "VIDIOC_STREAMON");
		return 0;
	}
	
	return 1;
}

int FrameGrabber::StopCapturing() 
{
	enum v4l2_buf_type type;
	
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type)) {
		fprintf(stderr, "VIDIOC_STREAMOFF");
		return 0;
	}
	
	return 1;
}

int FrameGrabber::Uninit()
{
	unsigned int i;
	for (i = 0; i < n_buffers; ++i)
		if (-1 == munmap (buffers[i].start, buffers[i].length)) {
			fprintf(stderr, "munmap!!!\n");
			return 0;
		}

	free (buffers);
	return 1;
}

bool FrameGrabber::GrabFrame(unsigned char* img)
{
	fd_set fds;
	struct timeval tv;
	int r;
	
	FD_ZERO (&fds);
	FD_SET (fd, &fds);
	
	// timeout
	tv.tv_sec = 0;
	tv.tv_usec = 10;
		
	// check if the next frame is ready
	r = select (fd + 1, &fds, NULL, NULL, &tv);

	if (-1 == r) {
		// next frame is not ready
		if (EINTR == errno)
			error_num = 2;
			return false;

		// select error
		error_num = 3;
		return false;
	}
	
	if (0 == r) {
		// select timeout
		error_num = 4;
		return false;
	}
	
	struct v4l2_buffer buf;
	
	CLEAR(buf);
	
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	
	if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
			case EAGAIN:
				error_num = 7;
				return false;
				
			case EIO:
				// Could ignore EIO, see spec
				// fall through
			default:
				// VIDIOC_DQBUF
				error_num = 5;
				return false;
					
		}
	}
	
	assert(buf.index < n_buffers);
	
	unsigned char* ptr = static_cast<unsigned char*>(buffers[buf.index].start);

	memcpy(img, ptr, height*width*pixel_size);
	
	if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
		// VIDIOC_QBUF
		error_num = 6;
		return false;
	}

	return true;
}
	

int FrameGrabber::xioctl(int fd, int  request, void* arg)
{
	int r;
	
	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);
	
	return r;
}
