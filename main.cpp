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

#include <iostream>

#include <osg/Texture2D>
#include <osg/ImageStream>
#include <osg/TextureRectangle>
#include <osgDB/ReadFile>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgGA/StateSetManipulator>
#include <osgGA/TrackballManipulator>

#include "framegrabber.h"

void YUV2Gray(unsigned char* gray, const unsigned char* yuv, const int width,
	const int height)
{
	for (int i=0; i<width*height; i++) {
		unsigned char y = yuv[i*2];
		gray[i] = y;
	}
}

int main(int argc, char** argv)
{
	const int width = 640;
	const int height = 480;

	FrameGrabber cam("/dev/video0", width, height, 40);
	cam.Init();
	
	osgViewer::Viewer viewer;
	viewer.setUpViewInWindow(0, 0, width, height);
	viewer.setCameraManipulator(new osgGA::TrackballManipulator());
	viewer.addEventHandler(new osgViewer::StatsHandler);
	viewer.addEventHandler(new osgViewer::ThreadingHandler);
	viewer.getCamera()->setClearMask(GL_DEPTH_BUFFER_BIT);
		
	osg::ref_ptr<osg::Geode> geode = new osg::Geode;
	osg::StateSet* stateset = geode->getOrCreateStateSet();
	stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
	
	osg::Geometry* geometry = osg::createTexturedQuadGeometry(
		osg::Vec3f(0,0,0),
		osg::Vec3f(width,0,0),
		osg::Vec3f(0,height,0),
		0, 0, 1, 1);
	
	unsigned char* tmp_yuv = (unsigned char*) 
								calloc(width*height*2, sizeof(unsigned char));
	unsigned char* tmp_mono = (unsigned char*) 
								calloc(width*height, sizeof(unsigned char));
	
	cam.StartCapturing();
	cam.GrabFrame(tmp_yuv);
	YUV2Gray(tmp_mono, tmp_yuv, width, height);
	
	osg::ImageStream* image_stream = new osg::ImageStream();
	image_stream->allocateImage(width,height,1,GL_LUMINANCE,GL_UNSIGNED_BYTE,1);
	image_stream->setDataVariance(osg::Object::DYNAMIC);
	
	image_stream->play();
	
	image_stream->setImage(width, height, 1, GL_TEXTURE_2D, GL_LUMINANCE,
						   GL_UNSIGNED_BYTE, tmp_mono, osg::Image::NO_DELETE);
	image_stream->flipVertical();
	image_stream->dirty();
	
	osg::Texture2D* texture = new osg::Texture2D(image_stream);
	texture->setInternalFormat(GL_RGB);
	texture->setDataVariance(osg::Object::DYNAMIC);
	texture->setResizeNonPowerOfTwoHint(false);
	texture->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
	texture->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
	
	geometry->getOrCreateStateSet()->
				setTextureAttributeAndModes(0, 
											texture,
											osg::StateAttribute::ON);
				
	geode->addDrawable(geometry);

	osg::ref_ptr<osg::Camera> orthoCamera = new osg::Camera;
	orthoCamera->setProjectionMatrix(osg::Matrix::ortho2D(0, width, 0, height));
	orthoCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
	orthoCamera->setViewMatrix(osg::Matrix::identity());
	orthoCamera->setViewport(0, 0, width, height);
	orthoCamera->setRenderOrder(osg::Camera::PRE_RENDER);
	orthoCamera->addChild(geode);
	
	osg::Group* root = new osg::Group();
	root->addChild(orthoCamera.get());
	root->addChild(osgDB::readNodeFile("cessna.osg"));
	
	viewer.setSceneData(root);
	viewer.assignSceneDataToCameras();
	
	while( !viewer.done() )
	{
		viewer.frame();
		if (cam.GrabFrame(tmp_yuv)) {
			YUV2Gray(tmp_mono, tmp_yuv, width, height);
			image_stream->setImage(width, height, 1, GL_TEXTURE_2D,
								   GL_LUMINANCE, GL_UNSIGNED_BYTE, tmp_mono,
								   osg::Image::NO_DELETE);
			image_stream->flipVertical();
		}
	}

	cam.StopCapturing();
	cam.Uninit();

	free(tmp_mono);
	free(tmp_yuv);
	
	return 0;
}
