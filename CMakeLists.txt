PROJECT(osg_ar)

ADD_EXECUTABLE(osg_ar main.cpp framegrabber.cpp)

target_link_libraries(osg_ar OpenThreads osg osgDB osgUtil osgGA osgViewer osgText osgGA osgUtil)
