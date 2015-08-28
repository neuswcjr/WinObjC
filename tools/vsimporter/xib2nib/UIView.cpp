//******************************************************************************
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//******************************************************************************

#include "UIView.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void InvertBool(struct _PropertyMapper *prop, NIBWriter *writer, XIBObject *propObj, XIBObject *obj)
{
    XIBObjectBool *value = (XIBObjectBool *) propObj;

    bool newVal = value->intValue() == 0;

    obj->AddOutputMember(writer, prop->nibName, new XIBObjectBool(newVal));
}

void ConvertNSFlags(struct _PropertyMapper *prop, NIBWriter *writer, XIBObject *propObj, XIBObject *obj)
{
}

static PropertyMapper propertyMappings[] = {
    "IBUIUserInteractionEnabled", "UIUserInteractionDisabled", InvertBool,
    "IBUITag", "UITag", NULL,
    "IBUIAlpha", "UIAlpha", NULL,
};
static const int numPropertyMappings = sizeof(propertyMappings) / sizeof(PropertyMapper);

UIView::UIView()
{
    memset(&_bounds, 0, sizeof(_bounds));
    memset(&_center, 0, sizeof(_center));
    memset(&_contentStretch, 0, sizeof(_contentStretch));
    _subviews = new XIBArray();
    _hidden = false;
    _opaque = true;
    _autoresizeSubviews = true;
    _autoresizingMask = 0;
    _multipleTouchEnabled = false;
    _clipsToBounds = false;
    _userInteractionDisabled = false;
    _contentMode = 0;
    _backgroundColor = NULL;
    _clearsContextBeforeDrawing = true;
    _enabled = true;
}

void UIView::InitFromXIB(XIBObject *obj)
{
    ObjectConverterSwapper::InitFromXIB(obj);
    _subviews = (XIBArray *) obj->FindMember("NSSubviews");
    if ( !_subviews ) _subviews = new XIBArray();
    if ( FindMember("NSFrame") != NULL ) {
        const char *pszFramePos = GetString("NSFrame", NULL);
        _bounds.x = 0;
        _bounds.y = 0;
        _bounds.width = 0;
        _bounds.height = 0;
        _center.x = 0;
        _center.y = 0;
        sscanf(pszFramePos, "{{%f, %f}, {%f, %f}}", &_center.x, &_center.y, &_bounds.width, &_bounds.height);
        _center.x += _bounds.width / 2.0f;
        _center.y += _bounds.height / 2.0f;
    } else {
        const char *pszFramePos = GetString("NSFrameSize", NULL);
        _bounds.x = 0;
        _bounds.y = 0;
        _bounds.width = 0;
        _bounds.height = 0;
        sscanf(pszFramePos, "{%f, %f}", &_bounds.width, &_bounds.height);
        _center.x = _bounds.width / 2.0f;
        _center.y = _bounds.height / 2.0f;
    }

    if ( FindMember("NSSuperview") == NULL ) {
        _center.x = _bounds.width / 2.0f;
        _center.y = _bounds.height / 2.0f;
    }

    _autoresizeSubviews = true;
    _autoresizingMask = 0;
    _hidden = false;
    _opaque = GetBool("IBUIOpaque", true);
    _clipsToBounds = GetBool("IBUIClipsSubviews", false);
    _userInteractionDisabled = !GetBool("IBUIUserInteractionEnabled", true);
    _contentMode = GetInt("IBUIContentMode", 0);
    _clearsContextBeforeDrawing = GetBool("IBUIClearsContextBeforeDrawing", true);

    if ( FindMember("NSvFlags") != NULL ) {
        int flags = FindMember("NSvFlags")->intValue();

        if ( flags & 0x100 ) {
            _autoresizeSubviews = true;
        }
        _autoresizingMask = flags & 0x3F;
        if ( flags & 0x80000000 ) {
            _hidden = true;
        }
    }
    _autoresizeSubviews = GetBool("IBUIAutoresizesSubviews", true);

    _enabled = GetBool("IBUIEnabled", true);
    _multipleTouchEnabled = GetBool("IBUIMultipleTouchEnabled", false);
    _backgroundColor = (UIColor *) FindMember("IBUIBackgroundColor");

    _outputClassName = "UIView";
}

void UIView::InitFromStory(XIBObject *obj)
{
    ObjectConverterSwapper::InitFromStory(obj);

    _subviews = (XIBArray *) obj->FindMemberClass("subviews");
    if ( !_subviews ) _subviews = new XIBArray();

    if ( getAttrib("opaque") ) {
        const char *pVal = getAttrib("opaque");
        if ( strcmp(pVal, "NO") == 0 ) _opaque = false;
    }
    if ( getAttrib("multipleTouchEnabled") ) {
        if ( strcmp(getAttrib("multipleTouchEnabled"), "YES") == 0 ) _multipleTouchEnabled = true;
    }
    if ( getAttrib("clipsSubviews") ) {
        if ( strcmp(getAttrib("clipsSubviews"), "YES") == 0 ) _clipsToBounds = true;
    }
    if ( getAttrib("userInteractionEnabled") ) {
        if ( strcmp(getAttrib("userInteractionEnabled"), "NO") == 0 ) _userInteractionDisabled = true;
    }
    if ( getAttrib("clearsContextBeforeDrawing") ) {
        if ( strcmp(getAttrib("clearsContextBeforeDrawing"), "NO") == 0 ) _clearsContextBeforeDrawing = false;
    }
    if ( getAttrib("contentMode") ) {
        const char *mode = getAttrib("contentMode");
        if ( strcmp(mode, "left") == 0 ) {
            _contentMode = UIViewContentModeLeft;
        } else if ( strcmp(mode, "scaleToFill") == 0 ) {
            _contentMode = UIViewContentModeScaleToFill;
        } else if ( strcmp(mode, "center") == 0 ) {
            _contentMode = UIViewContentModeCenter;
        } else if ( strcmp(mode, "redraw") == 0 ) {
            _contentMode = UIViewContentModeRedraw;
        } else if ( strcmp(mode, "scaleAspectFill") == 0 ) {
            _contentMode = UIViewContentModeScaleAspectFill;
        } else if ( strcmp(mode, "scaleAspectFit") == 0 ) {
            _contentMode = UIViewContentModeScaleAspectFit;
        } else {
            assert(0);
        }
    }
    if ( getAttrib("hidden") ) {
        if ( strcmp(getAttrib("hidden"), "YES") == 0 ) _hidden = true;
    }
    XIBObject *frameRect = FindMember("frame");
    if ( frameRect ) {
        _bounds.x = 0;
        _bounds.y = 0;
        _bounds.width = strtod(frameRect->getAttrib("width"), NULL);
        _bounds.height = strtod(frameRect->getAttrib("height"), NULL);

        _center.x = strtod(frameRect->getAttrib("x"), NULL);
        _center.y = strtod(frameRect->getAttrib("y"), NULL);

        _center.x += _bounds.width / 2.0f;
        _center.y += _bounds.height / 2.0f;
    }

    XIBObject *resizeMask = FindMember("autoresizingMask");
    if ( resizeMask ) {
        if ( resizeMask->getAttrib("widthSizable") ) _autoresizingMask |= (int) UIViewAutoresizingFlexibleWidth;
        if ( resizeMask->getAttrib("heightSizable") ) _autoresizingMask |= (int) UIViewAutoresizingFlexibleHeight;
        if ( resizeMask->getAttrib("flexibleMinX") ) _autoresizingMask |= (int) UIViewAutoresizingFlexibleLeftMargin;
        if ( resizeMask->getAttrib("flexibleMaxX") ) _autoresizingMask |= (int) UIViewAutoresizingFlexibleRightMargin;
        if ( resizeMask->getAttrib("flexibleMinY") ) _autoresizingMask |= (int) UIViewAutoresizingFlexibleTopMargin;
        if ( resizeMask->getAttrib("flexibleMaxY") ) _autoresizingMask |= (int) UIViewAutoresizingFlexibleBottomMargin;
    }
    _backgroundColor = (UIColor *) FindMember("backgroundColor");

    _outputClassName = "UIView";
}

void UIView::ConvertStaticMappings(NIBWriter *writer, XIBObject *obj)
{
    if ( !_ignoreUIObject ) writer->_allUIObjects->AddMember(NULL, this);

    if ( _subviews->count() > 0 ) {
        int count = _subviews->count();
        for ( int i = 0; i < count; i ++ ) {
            XIBObject *curObj = _subviews->objectAtIndex(i);
            if ( !curObj->_ignoreUIObject ) writer->_allUIObjects->AddMember(NULL, curObj);
        }
    }

    AddRect(writer, "UIBounds", _bounds);
    AddPoint(writer, "UICenter", _center);
    if ( _contentStretch.x != 0.0f || _contentStretch.y != 0.0f || _contentStretch.width != 0.0f || _contentStretch.height != 0.0f ) {
        AddRect(writer, "UIContentStretch", _contentStretch);
    }

    Map(writer, obj, propertyMappings, numPropertyMappings);
    if ( _subviews->count() > 0 ) AddOutputMember(writer, "UISubviews", _subviews);

    if ( _autoresizeSubviews ) AddBool(writer, "UIAutoresizeSubviews", _autoresizeSubviews);
    if ( _autoresizingMask ) AddInt(writer, "UIAutoresizingMask", _autoresizingMask);
    if ( _clipsToBounds ) AddBool(writer, "UIClipsToBounds", _clipsToBounds);
    if ( _backgroundColor ) AddOutputMember(writer, "UIBackgroundColor", _backgroundColor);
    if ( _contentMode ) AddInt(writer, "UIContentMode", _contentMode);
    if ( _userInteractionDisabled ) AddBool(writer, "UIUserInteractionDisabled", _userInteractionDisabled);
    if ( _hidden ) AddBool(writer, "UIHidden", _hidden);
    if ( !_enabled ) AddBool(writer, "UIDisabled", true);
    if ( _multipleTouchEnabled ) AddBool(writer, "UIMultipleTouchEnabled", _multipleTouchEnabled);
    if ( !_clearsContextBeforeDrawing ) AddBool(writer, "UIClearsContextBeforeDrawing", _clearsContextBeforeDrawing);

    if ( _opaque ) obj->AddBool(writer, "UIOpaque", _opaque);

    ObjectConverterSwapper::ConvertStaticMappings(writer, obj);
}

UIRect UIView::getFrame()
{
    UIRect curRect;

    curRect.width = _bounds.width;
    curRect.height = _bounds.height;
    curRect.x = _center.x - _bounds.width / 2.0f;
    curRect.y = _center.y - _bounds.height / 2.0f;

    return curRect;
}

void UIView::setFrame(UIRect frame)
{
    UIRect curRect, origRect;

    curRect.width = _bounds.width;
    curRect.height = _bounds.height;
    curRect.x = _center.x - _bounds.width / 2.0f;
    curRect.y = _center.y - _bounds.height / 2.0f;

    origRect = curRect;

    curRect = frame;

    if ( _autoresizeSubviews && _subviews != NULL ) {
        if ( curRect.width != origRect.width || curRect.height != origRect.height ) {
            CGSize delta, parentSize;

            delta.width = curRect.width - origRect.width;
            delta.height = curRect.height - origRect.height;
            parentSize.width = curRect.width;
            parentSize.height = curRect.height;

            int count = _subviews->count();

            //  Go through each subview and resize it
            for ( int i = 0; i < count; i ++ ) {
                UIView *subview = (UIView *) _subviews->objectAtIndex(i);

                if ( subview->_autoresizingMask == UIViewAutoresizingNone ) continue;
                
                UIRect curFrame, origFrame;
                curFrame = subview->getFrame();
                origFrame = curFrame;

                float left, middle, right, hdivision = 0.0f, numdivisions;

                left = curFrame.x;
                middle = curFrame.width;
                right = parentSize.width - (left + middle);

                numdivisions = 0.0f;
                if (subview->_autoresizingMask & UIViewAutoresizingFlexibleLeftMargin ) {
                    hdivision += left;
                    numdivisions += 1.0f;
                }
                if ( subview->_autoresizingMask & UIViewAutoresizingFlexibleWidth ) {
                    hdivision += middle;
                    numdivisions += 1.0f;
                }
                if ( subview->_autoresizingMask & UIViewAutoresizingFlexibleRightMargin ) {
                    hdivision += right;
                    numdivisions += 1.0f;
                }

                if (subview->_autoresizingMask & UIViewAutoresizingFlexibleLeftMargin ) {
                    if ( hdivision != left && hdivision != 0.0f ) {
                        curFrame.x += delta.width * left / hdivision;
                    } else {
                        curFrame.x += delta.width / numdivisions;
                    }
                }
                if ( subview->_autoresizingMask & UIViewAutoresizingFlexibleWidth ) {
                    if ( hdivision != middle && hdivision != 0.0f ) {
                        curFrame.width += delta.width * middle / hdivision;
                    } else {
                        curFrame.width += delta.width / numdivisions;
                    }
                }

                float top, height, bottom, vdivision = 0.0f;

                top = curFrame.y;
                height = curFrame.height;
                bottom = parentSize.height - (top + height);
                
                numdivisions = 0.0f;
                if (subview->_autoresizingMask & UIViewAutoresizingFlexibleTopMargin ) {
                    vdivision += top;
                    numdivisions += 1.0f;
                }
                if ( subview->_autoresizingMask & UIViewAutoresizingFlexibleHeight ) {
                    vdivision += height;
                    numdivisions += 1.0f;
                }
                if ( subview->_autoresizingMask & UIViewAutoresizingFlexibleBottomMargin ) {
                    vdivision += bottom;
                    numdivisions += 1.0f;
                }

                if (subview->_autoresizingMask & UIViewAutoresizingFlexibleTopMargin ) {
                    if ( vdivision != top && vdivision != 0.0f ) {
                        curFrame.y += delta.height * top / vdivision;
                    } else {
                        curFrame.y += delta.height / numdivisions;
                    }
                }
                if ( subview->_autoresizingMask & UIViewAutoresizingFlexibleHeight ) {
                    if ( vdivision != height && vdivision != 0.0f ) {
                        curFrame.height += delta.height * height / vdivision;
                    } else {
                        curFrame.height += delta.height / numdivisions;
                    }
                }

                curFrame.x = floorf(curFrame.x);
                curFrame.y = floorf(curFrame.y);
                curFrame.width = ceilf(curFrame.width);
                curFrame.height = ceilf(curFrame.height);

                subview->setFrame(curFrame);
            }
        }
    }

    _bounds.width = curRect.width;
    _bounds.height = curRect.height;
    _center.x = curRect.x + curRect.width / 2.0f;
    _center.y = curRect.y + curRect.height / 2.0f;
}

