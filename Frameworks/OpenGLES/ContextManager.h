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

#pragma once

#include "Hash.h"

struct threadGLContext
{
    id curEAGLContext;
};

extern __declspec(thread) threadGLContext *tlsCurContext;
class ContextManager
{
public:
    void setContext(id ctx, bool doRef = true);
    EAGLContext *getEAGLContext();

    bool lockContext();
    void unlockContext();
};

extern ContextManager ctxManager;
namespace OpenGLES
{
    class OpenGLESContext;
}

class DisplayTextureD3D;

struct EAGLContextPrivateData
{
    int         contextHandle;
    EbrLock     contextLock;

    CAEAGLLayer* presentationLayer;
	DisplayTextureD3D* _d3dSurface;
    GLuint      texNum;
    id sharedContext;
    id          _sharegroup;
    bool        _opaque;
    int         _curRenderbufferBinding;        //  Some gpus don't seem to support glGetInteger(GL_(RENDERBUFFER|FRAMEBUFFER)_BINDING_OES)
    int         _curFramebufferBinding;

    GLuint      _drawRenderbuffer, _flipRenderbuffer, _curRenderbufferTarget;
    GLuint      _deleteRenderbuffer;
    GLuint      _boundRenderbufferFramebuffer;
    bool        _targetFlip;
    HashMap<DWORD, DWORD> *_ownedFramebuffers;
    int         _rbWidth, _rbHeight;

    OpenGLES::OpenGLESContext *_gl11Ctx;
};
