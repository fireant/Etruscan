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

#ifndef FRAMEGRABBER_H
#define FRAMEGRABBER_H

#include <string>

struct buffer {
	void*  start;
	size_t length;
};

class FrameGrabber
{
public:
	FrameGrabber(const std::string device_name, const int width,
				 const int height, const int fps=30, const bool freq_50hz=false, 
				 const int num_buffers=5);
	int Init();
	int StartCapturing();
	int StopCapturing();
	int Uninit();
	bool GrabFrame(unsigned char* img);
	~FrameGrabber();
	
	int error_num;

private:
	int	xioctl(int fd, int  request, void* arg);
	
	std::string dev_name;
	int fd;
	unsigned int n_buffers;
	int failed;
	int width;
	int height;
	int pixel_size;
	int frame_rate;
	bool power_line_freq50;
	
	struct buffer* buffers;
};

#endif // FRAMEGRABBER_H
