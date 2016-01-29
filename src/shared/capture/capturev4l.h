//========================================================================
//  This software is free: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License Version 3,
//  as published by the Free Software Foundation.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  Version 3 in the file COPYING that came with this distribution.
//  If not, see <http://www.gnu.org/licenses/>.
//
//========================================================================
// Some portions of software interfacing with XIO originally writtten
// by James Bruce as part of the CMVision library.
//      http://www.cs.cmu.edu/~jbruce/cmvision/
// (C) 2004-2006 James R. Bruce, Carnegie Mellon University
// Licenced under the GNU General Public License (GPL) version 2,
//   or alternately by a specific written agreement.
//========================================================================
// (C) 2016 Eric Zavesky, Emoters
// Licenced under the GNU General Public License (GPL) version 3,
//   or alternately by a specific written agreement.
//========================================================================
/*!
  \file    capturev4l.h
  \brief   C++ Interface: CaptureV4L
  \author  Eric Zavesky, (C) 2016 (derived from DC1394 and CMVision)
*/
//========================================================================

#ifndef CAPTUREV4l_H
#define CAPTUREV4l_H
#include "captureinterface.h"
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "VarTypes.h"
#include <linux/videodev2.h>
#include <sys/poll.h>

//#include "conversions.h"
#ifndef VDATA_NO_QT
  #include <QMutex>
#else
  #include <pthread.h>
#endif

typedef long v4lfeature_t;

// if you get a message like "DQBUF returned error", "DQBUF error: invalid"
// then you need to use a higher value for STREAMBUFS or process frames faster
#define V4L_STREAMBUFS            3

// number of usb devices to scan as potential camera sources
#define MAX_CAM_SCAN                4

/*!
  \class GlobalV4Linstance
  \brief A singleton provider of a v4l lib context used for capturing with multiple threads
  \author  Eric Zavesky, (C) 2016 (derived from DC1394 interface)
*/
class GlobalV4Linstance
{
public:
    struct image_t {
        unsigned char *data;
        size_t length;
        int index;
        uint8_t  field;        // which field of video this is from {0,1}
        timeval  timestamp;
    };
    enum private_control_t {    //custom control codes that must be individually caught...
        V4L2_FEATURE_PRIVATE = V4L2_CID_PRIVATE_BASE,
        V4L2_FEATURE_FRAME_RATE
    };
public:
  GlobalV4Linstance() {
    #ifndef VDATA_NO_QT
    #else
      pthread_mutex_init (&mutex);
    #endif
    counter=0;
    pollset.fd=-1;
    memset(szDevice, 0, sizeof(char)*128);
  }
  ~GlobalV4Linstance() {
      removeInstance();
    #ifndef VDATA_NO_QT
    #else
      pthread_mutex_destroy (&mutex);
    #endif
  }
  #ifndef VDATA_NO_QT
    QMutex mutex;
  #else
    pthread_mutex_t mutex;
  #endif
protected:
    pollfd pollset;
    char counter;
    char szDevice[128];
    struct v4l2_buffer tempbuf;
    image_t img[V4L_STREAMBUFS];

    bool enqueueBuffer(v4l2_buffer &buf);
    bool dequeueBuffer(v4l2_buffer &buf);
    bool waitForFrame(int max_msec=500);

public:
    bool obtainInstance(int iDevice);
    bool obtainInstance(char *szDevice);
    bool removeInstance(bool bRelock=true);
    
    bool xioctl(int request,void *data, const char *error_str);
    static int xioctl(int fd,int request,void *data, const char *error_str);
    
    bool getControl(int ctrl_id,long &s);
    bool setControl(int ctrl_id,long s);
    bool checkControl(int ctrl_id, bool *bEnabled=NULL, bool *bReadOnly=NULL,
                      long *lDefault=NULL, long *lMin=NULL, long *lMax=NULL);
    bool startStreaming(int iWidth_, int iHeight_, int iInput=0);
    bool stopStreaming();
    
    const image_t *captureFrame(int iMaxSpin=1);
    bool releaseFrame(const image_t *_img);
    
    int enumerateCameras(int *id_list, int max_id=4);

private:
  void lock() {
    #ifndef VDATA_NO_QT
      mutex.lock();
    #else
      pthread_mutex_lock();
    #endif
  }
  void unlock() {
    #ifndef VDATA_NO_QT
      mutex.unlock();
    #else
      pthread_mutex_unlock();
    #endif
  }

};


/*!
  \class GlobalV4LinstanceManager
  \brief A static instance manager to provide global singleton access to GlobalV4Linstance
  \author  Stefan Zickler, (C) 2008
*/
class GlobalV4LinstanceManager
  {
  public:
      static GlobalV4Linstance* obtainInstance();
      static bool removeInstance();
  protected:
      GlobalV4LinstanceManager();
      ~GlobalV4LinstanceManager();
      GlobalV4LinstanceManager(const GlobalV4LinstanceManager&);
      GlobalV4LinstanceManager& operator= (const GlobalV4LinstanceManager&);
  private:
      static GlobalV4LinstanceManager* pinstance;
      GlobalV4Linstance instance;
 };



/*!
  \class CaptureV4L
  \brief A v4l-based USB/Video For Linux Capture Class
  \author  Eric Zavesky, (C) 2016

  This class mirrors popular v4l implementations giving the most 
  compatible options and based on the DC1394 base classes by Stefan.

  Overall, it not only provides capture-abilities, but also full
  on-the-fly configuration abilities through the VarTypes system.
  
  If you find your camera not working correctly, or discover a bug,
  please inform the author, but also consider contributing yourself,
  so that we can cover as many options as possible.
*/
#ifndef VDATA_NO_QT
  #include <QMutex>
  //if using QT, inherit QObject as a base
class CaptureV4L : public QObject, public CaptureInterface
#else
class CaptureV4L : public CaptureInterface
#endif
{
#ifndef VDATA_NO_QT
    Q_OBJECT
    public slots:
    void changed(VarType * group);
protected:
    QMutex mutex;
public:
#endif
    
    
protected:
    bool is_capturing;
    
    //processing variables:
    VarStringEnum * v_colorout;
    
    //DCAM parameters:
    VarList * P_BRIGHTNESS;
    VarList * P_CONTRAST;
    VarList * P_EXPOSURE;
    VarList * P_SHARPNESS;
    VarList * P_WHITE_BALANCE;
    VarList * P_HUE;
    VarList * P_SATURATION;
    VarList * P_GAMMA;
    VarList * P_GAIN;
    VarList * P_TEMPERATURE;  // white balance temperature
    VarList * P_FRAME_RATE;
    
    //capture variables:
    VarInt    * v_cam_bus;
    VarInt    * v_fps;
    VarInt    * v_width;
    VarInt    * v_height;
    VarInt    * v_left;
    VarInt    * v_top;
    VarStringEnum * v_colormode;
    VarStringEnum * v_format;
    VarInt    * v_buffer_size;
    
    unsigned int cam_id;
    int width;
    int height;
    int top;
    int left;
    ColorFormat capture_format;
    int ring_buffer_size;
    int cam_list[MAX_CAM_SCAN];
    int cam_count;
    
    GlobalV4Linstance * camera_instance;
//    dc1394framerate_t dcfps;
//    dc1394video_mode_t dcformat;
//    dc1394featureset_t features;
    const GlobalV4Linstance::image_t *_img;
    //dc1394camera_t **cameras;
    //dc1394camera_t * camera;
    
    VarList * dcam_parameters;
    VarList * capture_settings;
    VarList * conversion_settings;
    
public:
#ifndef VDATA_NO_QT
    CaptureV4L(VarList * _settings=0,int default_camera_id=0,QObject * parent=0);
    void mvc_connect(VarList * group);
#else
    CaptureV4L(VarList * _settings=0,int default_camera_id=0);
#endif
    ~CaptureV4L();
    
    /// Initialize the interface and start capture
    virtual bool startCapture();
    
    /// Stop Capture
    virtual bool stopCapture();
    
    virtual bool isCapturing() { return is_capturing; };
    
    /// This function converts a local variable pointer
    /// to a DC1394 feature enum
    v4lfeature_t getV4LfeatureEnum(VarList * val, bool & valid);
    v4lfeature_t getV4LfeatureEnum(VarList * val) {
        bool b=true;
        return getV4LfeatureEnum(val,b);
    }
    
    /// This function converts a DC1394 feature into a
    /// local variable Pointer
    VarList * getVariablePointer(v4lfeature_t val);
    
    /// this gives a raw-image with a pointer directly to the video-buffer
    /// Note that this pointer is only guaranteed to point to a valid
    /// memory location until releaseFrame() is called.
    virtual RawImage getFrame();
    
    virtual void releaseFrame();
    
    virtual bool resetBus();
    
    void cleanup();
    
    void readParameterValues(VarList * item);
    
    void writeParameterValues(VarList * item);
    
    void readParameterProperty(VarList * item);
    
    void readAllParameterProperties();
    
    /// will perform a pure memcpy of an image, independent of vartypes
    /// conversion settings
    bool copyFrame(const RawImage & src, RawImage & target);
    
    virtual void readAllParameterValues();
    
    void writeAllParameterValues();
    
    virtual bool copyAndConvertFrame(const RawImage & src, RawImage & target);
    
    virtual string getCaptureMethodName() const;
    
protected:
    /// This function is used internally only
    /// The user should call copyAndConvertFrame with parameters setup through the
    /// VarTypes settings.
    bool convertFrame(const RawImage & src, RawImage & target,
                      ColorFormat output_fmt, int y16bits=16);
    
    
};

#endif
