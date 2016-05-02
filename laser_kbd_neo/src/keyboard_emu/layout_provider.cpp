/*
 *  Keyboard layout manager and provider
 *
 *  By Shikai Chen (csk@live.com)
 *  
 *  Copyright 2009 - 2013 RoboPeak Team
 *  http://www.robopeak.net
 */

#if defined(WIN32) || defined(_MACOS)
#define FEATURE_PATTERN_TYPE_2
#else
#include "../config.h"
#endif
#include "../common.h"
#include "../cv_common.h"
#include <algorithm>
#include "layout_provider.h"
#include <functional>
#include "kdtree++/kdtree.hpp"
#pragma comment(lib, "ws2_32.lib") // Winsock Library

#define SERVER "127.0.0.1"
#define PORT 8000

static const float MAX_KEY_BUTTON_SIZE = 60.0f;

const static KeyDesc_t _key_mapper[] =
{ 
    //    X       Y      W      H    Val     Val(capped)  Disp
    {      0,	   0,    100,    100,   'E',    'E',  "E"} ,
    {    101,      0,    100,    100,   'R',    'R',  "R"} ,
    {      0,    101,    100,    100,   'T',    'T',  "T"} ,
    {    101,    101,    100,    100,   'Y',    'Y',  "Y"} 
};


struct KDBlob {
   float _x;
   float _y;
   int   _id;

   KDBlob(int id, float x, float y){
        _id = id;
        _x  = x;
        _y = y;
   }

   float operator[](size_t pos) const{
        if (pos == 0) return _x;
        else return _y;
   }
};

struct kdblob_evaluator
{
    bool operator() ( const KDBlob & blob) const {
        return KeyLayoutProvider::IsButtonHit(_x,
            _y, _key_mapper[blob._id]);
    }
    kdblob_evaluator(float x, float y) {
        _x = x;
        _y = y;
    }


    float _x, _y;
};

static float kdblob_accessor(const KDBlob  & val, size_t pos) {
    return val[pos];
}

typedef KDTree::KDTree<2, KDBlob, std::pointer_to_binary_function<const KDBlob &,size_t,float> > kdtree_2d_t;

#define GET_KD_TREE() (reinterpret_cast<kdtree_2d_t *>(_kd_tree))

KeyLayoutProvider::KeyLayoutProvider()
{
	/* INITIALIZE CLIENT UDP SOCKET FOR TRANSMITTING KEY STROKES */
	_slen = sizeof(_si_other);

	//Initialise winsock
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &_wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");

	//create socket
	if ((_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
	{
		printf("socket() failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	//setup address structure
	memset((char *)&_si_other, 0, sizeof(_si_other));
	_si_other.sin_family = AF_INET;
	_si_other.sin_port = htons(PORT);
	_si_other.sin_addr.S_un.S_addr = inet_addr(SERVER);

    _id_tl = _id_tr = _id_bl = _id_br = 0;
    
    _kd_tree = new kdtree_2d_t(std::ptr_fun(kdblob_accessor));
    

    for (int pos = 0; pos<_countof(_key_mapper); ++pos)
    {
        KDBlob blob(pos, _key_mapper[pos].x, _key_mapper[pos].y);
        GET_KD_TREE()->insert(blob);

        
        // find top-left key
        if (_key_mapper[_id_tl].x >= _key_mapper[pos].x
            && _key_mapper[_id_tl].y >= _key_mapper[pos].y) {
            _id_tl = pos;
        }

        // find top-right key
        if (_key_mapper[_id_tr].x <= _key_mapper[pos].x
            && _key_mapper[_id_tr].y >= _key_mapper[pos].y) {
            _id_tr = pos;
        }

        // find bottom-left key
        if (_key_mapper[_id_bl].x >= _key_mapper[pos].x
            && _key_mapper[_id_bl].y <= _key_mapper[pos].y) {
            _id_bl = pos;
        }

        // find bottom-right key
        if (_key_mapper[_id_br].x <= _key_mapper[pos].x
            && _key_mapper[_id_br].y <= _key_mapper[pos].y) {
            _id_br = pos;
        }
    }
 
    // buildsize
    size_t width1 = _key_mapper[_id_tl].width/2 + 
                    _key_mapper[_id_tr].width/2 +
                    _key_mapper[_id_tr].x - _key_mapper[_id_tl].x;

    size_t width2 = _key_mapper[_id_bl].width/2 + 
                    _key_mapper[_id_br].width/2 +
                    _key_mapper[_id_br].x - _key_mapper[_id_bl].x;

    size_t height1 = _key_mapper[_id_tl].height/2 + 
                    _key_mapper[_id_bl].height/2 +
                    _key_mapper[_id_bl].y - _key_mapper[_id_tl].y;

    size_t height2 = _key_mapper[_id_tr].height/2 + 
                    _key_mapper[_id_br].height/2 +
                    _key_mapper[_id_br].y - _key_mapper[_id_tr].y;

    _size.width = std::max(width1, width2);
    _size.height = std::max(height1, height2);
}

KeyLayoutProvider::~KeyLayoutProvider()
{
    delete GET_KD_TREE();
}

const KeyDesc_t * KeyLayoutProvider::getKeyAt(int pos)
{
    if (pos >= getCount()) return NULL;
    return &_key_mapper[pos];
}

size_t KeyLayoutProvider::getCount()
{
    return _countof(_key_mapper);
}

int KeyLayoutProvider::findKeyIdxByPoint(float x, float y)
{

    KDBlob searchBlob(-1, x, y);
    std::pair<kdtree_2d_t::const_iterator,float> nif =  GET_KD_TREE()->find_nearest_if(searchBlob, MAX_KEY_BUTTON_SIZE,
        kdblob_evaluator(x,y));

    if (nif.first == GET_KD_TREE()->end()) {
        return -1;
    }


    return nif.first->_id;
}
int  KeyLayoutProvider::getKeyIdxByKeyVal(int ch)
{
    
    for (int pos = 0; pos < _countof(_key_mapper); ++pos) {
        if ( _key_mapper[pos].lcase_val == ch) {
            return pos;
        }
    }
    return -1;
}

bool KeyLayoutProvider::IsButtonHit(float x, float y, const KeyDesc_t & key)
{
    float fixed_width;
    fixed_width = key.width + 3;
    //y += 5;// make some post-fix
    if ( x>=key.x-fixed_width*5/12 && x < key.x+fixed_width*5/12) {
        if (y>=key.y-key.height*5/12 && y< key.y + key.height*5/12) {
            return true;
        }
    }
    return false;
}

CvPoint2D32f KeyLayoutProvider::keyboardPosToWindowPos(const CvPoint2D32f & src)
{
    return cvPoint2D32f(src.x - std::min(_key_mapper[_id_tl].x - _key_mapper[_id_tl].width/2, _key_mapper[_id_bl].x - _key_mapper[_id_bl].width/2)
        , src.y);
}

void KeyLayoutProvider::renderLayout(IplImage* img, const std::vector<int> & key_pressed)
{
    const int imgWidth = img->roi->width;
    const int imgHeight = img->roi->height;

    const float MARGIN_VAL = 2;

    float offsetX = std::min(_key_mapper[_id_tl].x - _key_mapper[_id_tl].width/2, _key_mapper[_id_bl].x - _key_mapper[_id_bl].width/2);
    float offsetY = std::min(_key_mapper[_id_tl].y - _key_mapper[_id_tl].height/2, _key_mapper[_id_tr].y - _key_mapper[_id_tr].height/2);

    float scale = 1;

    scale = (imgHeight - MARGIN_VAL*2) / _size.height;

    if (_size.width * scale > imgWidth - MARGIN_VAL*2) {
        scale = (imgWidth - MARGIN_VAL*2) / _size.width;
    }

    offsetX -= (imgWidth/scale - _size.width)/2;
    offsetY -= (imgHeight/scale - _size.height)/2;

    cvFillImage(img, 0x0);

     // draw Layout
    for (int pos = 0; pos < _countof(_key_mapper); ++pos)
    {
        float posX, posY;
        float width, height;
        
        posX = _key_mapper[pos].x -  offsetX;
        posY = _key_mapper[pos].y -  offsetY;
        width = _key_mapper[pos].width -2;
        height = _key_mapper[pos].height -2;

        posX*=scale;
        posY*=scale;
        width*=scale;
        height*=scale;

        cvDrawRect( img, cvPoint(posX-width/2, posY-height/2) ,
                cvPoint(posX+width/2, posY+height/2), cvScalar(120,120,120));
        cv_textOut(img, posX-width/3, posY, _key_mapper[pos].desc, cvScalar(100,100,100));

    }


    // draw pressed
    for (int pos = 0; pos < key_pressed.size(); ++pos)
    {
        float posX, posY;
        float width, height;
        
        posX = _key_mapper[key_pressed[pos]].x -  offsetX;
        posY = _key_mapper[key_pressed[pos]].y -  offsetY;
        width = _key_mapper[key_pressed[pos]].width -2;
        height = _key_mapper[key_pressed[pos]].height -2;

        posX*=scale;
        posY*=scale;
        width*=scale;
        height*=scale;

        cvDrawRect( img, cvPoint(posX-width/2, posY-height/2) ,
                cvPoint(posX+width/2, posY+height/2), cvScalar(0,255,255));
        cv_textOut(img, posX-width/3, posY, _key_mapper[key_pressed[pos]].desc, cvScalar(0,255,255));

    }
}
   
