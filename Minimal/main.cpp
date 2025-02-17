/************************************************************************************

Authors     :   Bradley Austin Davis <bdavis@saintandreas.org>
Copyright   :   Copyright Brad Davis. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include <iostream>
#include <memory>
#include <exception>
#include <algorithm>

#include <Windows.h>

#define __STDC_FORMAT_MACROS 1

#define FAIL(X) throw std::runtime_error(X)

///////////////////////////////////////////////////////////////////////////////
//
// GLM is a C++ math library meant to mirror the syntax of GLSL 
//

#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include "Skybox.h"

// Import the most commonly used types into the default namespace
using glm::ivec3;
using glm::ivec2;
using glm::uvec2;
using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::quat;

///////////////////////////////////////////////////////////////////////////////
//
// GLEW gives cross platform access to OpenGL 3.x+ functionality.  
//

#include <GL/glew.h>

bool checkFramebufferStatus(GLenum target = GL_FRAMEBUFFER)
{
  GLuint status = glCheckFramebufferStatus(target);
  switch (status)
  {
  case GL_FRAMEBUFFER_COMPLETE:
    return true;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
    std::cerr << "framebuffer incomplete attachment" << std::endl;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
    std::cerr << "framebuffer missing attachment" << std::endl;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
    std::cerr << "framebuffer incomplete draw buffer" << std::endl;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
    std::cerr << "framebuffer incomplete read buffer" << std::endl;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
    std::cerr << "framebuffer incomplete multisample" << std::endl;
    break;

  case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
    std::cerr << "framebuffer incomplete layer targets" << std::endl;
    break;

  case GL_FRAMEBUFFER_UNSUPPORTED:
    std::cerr << "framebuffer unsupported internal format or image" << std::endl;
    break;

  default:
    std::cerr << "other framebuffer error" << std::endl;
    break;
  }

  return false;
}

bool checkGlError()
{
  GLenum error = glGetError();
  if (!error)
  {
    return false;
  }
  else
  {
    switch (error)
    {
    case GL_INVALID_ENUM:
      std::cerr <<
        ": An unacceptable value is specified for an enumerated argument.The offending command is ignored and has no other side effect than to set the error flag.";
      break;
    case GL_INVALID_VALUE:
      std::cerr <<
        ": A numeric argument is out of range.The offending command is ignored and has no other side effect than to set the error flag";
      break;
    case GL_INVALID_OPERATION:
      std::cerr <<
        ": The specified operation is not allowed in the current state.The offending command is ignored and has no other side effect than to set the error flag..";
      break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      std::cerr <<
        ": The framebuffer object is not complete.The offending command is ignored and has no other side effect than to set the error flag.";
      break;
    case GL_OUT_OF_MEMORY:
      std::cerr <<
        ": There is not enough memory left to execute the command.The state of the GL is undefined, except for the state of the error flags, after this error is recorded.";
      break;
    case GL_STACK_UNDERFLOW:
      std::cerr <<
        ": An attempt has been made to perform an operation that would cause an internal stack to underflow.";
      break;
    case GL_STACK_OVERFLOW:
      std::cerr << ": An attempt has been made to perform an operation that would cause an internal stack to overflow.";
      break;
    }
    return true;
  }
}

void glDebugCallbackHandler(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* msg,
                            GLvoid* data)
{
  OutputDebugStringA(msg);
  std::cout << "debug call: " << msg << std::endl;
}

//////////////////////////////////////////////////////////////////////
//
// GLFW provides cross platform window creation
//

#include <GLFW/glfw3.h>

namespace glfw
{
  inline GLFWwindow* createWindow(const uvec2& size, const ivec2& position = ivec2(INT_MIN))
  {
    GLFWwindow* window = glfwCreateWindow(size.x, size.y, "glfw", nullptr, nullptr);
    if (!window)
    {
      FAIL("Unable to create rendering window");
    }
    if ((position.x > INT_MIN) && (position.y > INT_MIN))
    {
      glfwSetWindowPos(window, position.x, position.y);
    }
    return window;
  }
}

// A class to encapsulate using GLFW to handle input and render a scene
class GlfwApp
{
protected:
  uvec2 windowSize;
  ivec2 windowPosition;
  GLFWwindow* window{nullptr};
  unsigned int frame{0};

public:
  GlfwApp()
  {
    // Initialize the GLFW system for creating and positioning windows
    if (!glfwInit())
    {
      FAIL("Failed to initialize GLFW");
    }
    glfwSetErrorCallback(ErrorCallback);
  }

  virtual ~GlfwApp()
  {
    if (nullptr != window)
    {
      glfwDestroyWindow(window);
    }
    glfwTerminate();
  }

  virtual int run()
  {
    preCreate();

    window = createRenderingTarget(windowSize, windowPosition);

    if (!window)
    {
      std::cout << "Unable to create OpenGL window" << std::endl;
      return -1;
    }

    postCreate();

    initGl();

    while (!glfwWindowShouldClose(window))
    {
      ++frame;
      glfwPollEvents();
      update();
      draw();
      finishFrame();
    }

    shutdownGl();

    return 0;
  }

protected:
  virtual GLFWwindow* createRenderingTarget(uvec2& size, ivec2& pos) = 0;

  virtual void draw() = 0;

  void preCreate()
  {
    glfwWindowHint(GLFW_DEPTH_BITS, 16);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
  }

  void postCreate()
  {
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwMakeContextCurrent(window);

    // Initialize the OpenGL bindings
    // For some reason we have to set this experminetal flag to properly
    // init GLEW if we use a core context.
    glewExperimental = GL_TRUE;
    if (0 != glewInit())
    {
      FAIL("Failed to initialize GLEW");
    }
    glGetError();

    if (GLEW_KHR_debug)
    {
      GLint v;
      glGetIntegerv(GL_CONTEXT_FLAGS, &v);
      if (v & GL_CONTEXT_FLAG_DEBUG_BIT)
      {
        //glDebugMessageCallback(glDebugCallbackHandler, this);
      }
    }
  }

  virtual void initGl()
  {
  }

  virtual void shutdownGl()
  {
  }

  virtual void finishFrame()
  {
    glfwSwapBuffers(window);
  }

  virtual void destroyWindow()
  {
    glfwSetKeyCallback(window, nullptr);
    glfwSetMouseButtonCallback(window, nullptr);
    glfwDestroyWindow(window);
  }

  virtual void onKey(int key, int scancode, int action, int mods)
  {
    if (GLFW_PRESS != action)
    {
      return;
    }

    switch (key)
    {
    case GLFW_KEY_ESCAPE:
      glfwSetWindowShouldClose(window, 1);
      return;
    }
  }

  virtual void update()
  {
  }

  virtual void onMouseButton(int button, int action, int mods)
  {
  }

protected:
  virtual void viewport(const ivec2& pos, const uvec2& size)
  {
    glViewport(pos.x, pos.y, size.x, size.y);
  }

private:

  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
  {
    GlfwApp* instance = (GlfwApp *)glfwGetWindowUserPointer(window);
    instance->onKey(key, scancode, action, mods);
  }

  static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
  {
    GlfwApp* instance = (GlfwApp *)glfwGetWindowUserPointer(window);
    instance->onMouseButton(button, action, mods);
  }

  static void ErrorCallback(int error, const char* description)
  {
    FAIL(description);
  }
};

//////////////////////////////////////////////////////////////////////
//
// The Oculus VR C API provides access to information about the HMD
//

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>

namespace ovr
{
  // Convenience method for looping over each eye with a lambda
  template <typename Function>
  inline void for_each_eye(Function function)
  {
    for (ovrEyeType eye = ovrEyeType::ovrEye_Left;
         eye < ovrEyeType::ovrEye_Count;
         eye = static_cast<ovrEyeType>(eye + 1))
    {
      function(eye);
    }
  }

  inline mat4 toGlm(const ovrMatrix4f& om)
  {
    return glm::transpose(glm::make_mat4(&om.M[0][0]));
  }

  inline mat4 toGlm(const ovrFovPort& fovport, float nearPlane = 0.01f, float farPlane = 10000.0f)
  {
    return toGlm(ovrMatrix4f_Projection(fovport, nearPlane, farPlane, true));
  }

  inline vec3 toGlm(const ovrVector3f& ov)
  {
    return glm::make_vec3(&ov.x);
  }

  inline vec2 toGlm(const ovrVector2f& ov)
  {
    return glm::make_vec2(&ov.x);
  }

  inline uvec2 toGlm(const ovrSizei& ov)
  {
    return uvec2(ov.w, ov.h);
  }

  inline quat toGlm(const ovrQuatf& oq)
  {
    return glm::make_quat(&oq.x);
  }

  inline mat4 toGlm(const ovrPosef& op)
  {
    mat4 orientation = glm::mat4_cast(toGlm(op.Orientation));
    mat4 translation = glm::translate(mat4(), ovr::toGlm(op.Position));
    return translation * orientation;
  }

  inline ovrMatrix4f fromGlm(const mat4& m)
  {
    ovrMatrix4f result;
    mat4 transposed(glm::transpose(m));
    memcpy(result.M, &(transposed[0][0]), sizeof(float) * 16);
    return result;
  }

  inline ovrVector3f fromGlm(const vec3& v)
  {
    ovrVector3f result;
    result.x = v.x;
    result.y = v.y;
    result.z = v.z;
    return result;
  }

  inline ovrVector2f fromGlm(const vec2& v)
  {
    ovrVector2f result;
    result.x = v.x;
    result.y = v.y;
    return result;
  }

  inline ovrSizei fromGlm(const uvec2& v)
  {
    ovrSizei result;
    result.w = v.x;
    result.h = v.y;
    return result;
  }

  inline ovrQuatf fromGlm(const quat& q)
  {
    ovrQuatf result;
    result.x = q.x;
    result.y = q.y;
    result.z = q.z;
    result.w = q.w;
    return result;
  }
}

class RiftManagerApp
{
protected:
  ovrSession _session;
  ovrHmdDesc _hmdDesc;
  ovrGraphicsLuid _luid;

public:
  RiftManagerApp()
  {
    if (!OVR_SUCCESS(ovr_Create(&_session, &_luid)))
    {
      FAIL("Unable to create HMD session");
    }

    _hmdDesc = ovr_GetHmdDesc(_session);
  }

  ~RiftManagerApp()
  {
    ovr_Destroy(_session);
    _session = nullptr;
  }
};

class RiftApp : public GlfwApp, public RiftManagerApp
{
public:
	ovrInputState inputState;
	int a_pressed = 0;
	bool a_hasPressed = false;
	float iod;

private:
  GLuint _fbo{0};
  GLuint _depthBuffer{0};
  ovrTextureSwapChain _eyeTexture;

  GLuint _mirrorFbo{0};
  ovrMirrorTexture _mirrorTexture;

  ovrEyeRenderDesc _eyeRenderDescs[2];

  mat4 _eyeProjections[2];

  ovrLayerEyeFov _sceneLayer;
  ovrViewScaleDesc _viewScaleDesc;

  uvec2 _renderTargetSize;
  uvec2 _mirrorSize;

public:

  RiftApp()
  {
    using namespace ovr;
    _viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;

    memset(&_sceneLayer, 0, sizeof(ovrLayerEyeFov));
    _sceneLayer.Header.Type = ovrLayerType_EyeFov;
    _sceneLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;

    ovr::for_each_eye([&](ovrEyeType eye)
    {
      ovrEyeRenderDesc& erd = _eyeRenderDescs[eye] = ovr_GetRenderDesc(_session, eye, _hmdDesc.DefaultEyeFov[eye]);
      ovrMatrix4f ovrPerspectiveProjection =
        ovrMatrix4f_Projection(erd.Fov, 0.01f, 1000.0f, ovrProjection_ClipRangeOpenGL);
      _eyeProjections[eye] = ovr::toGlm(ovrPerspectiveProjection);
      _viewScaleDesc.HmdToEyePose[eye] = erd.HmdToEyePose;

      ovrFovPort& fov = _sceneLayer.Fov[eye] = _eyeRenderDescs[eye].Fov;
      auto eyeSize = ovr_GetFovTextureSize(_session, eye, fov, 1.0f);
      _sceneLayer.Viewport[eye].Size = eyeSize;
      _sceneLayer.Viewport[eye].Pos = {(int)_renderTargetSize.x, 0};

      _renderTargetSize.y = std::max(_renderTargetSize.y, (uint32_t)eyeSize.h);
      _renderTargetSize.x += eyeSize.w;
    });

	//Set default value of iod in the constructor of RiftApp
	iod = std::abs(_viewScaleDesc.HmdToEyePose[0].Position.x - _viewScaleDesc.HmdToEyePose[1].Position.x);
	std::cout << iod << std::endl;
    // Make the on screen window 1/4 the resolution of the render target
    _mirrorSize = _renderTargetSize;
    _mirrorSize /= 4;
  }

  void setIOD(float iodOffset) {
	   float newIOD = iod + iodOffset;
	
	  if (newIOD < -0.1f) {
		  newIOD = -0.1f;
	  }

	  if (newIOD > 0.3f) {
		  newIOD = 0.3f;
	  }
	  _viewScaleDesc.HmdToEyePose[0].Position.x = -newIOD / 2.0f;
	  _viewScaleDesc.HmdToEyePose[1].Position.x = newIOD / 2.0f;
  }

protected:
  GLFWwindow* createRenderingTarget(uvec2& outSize, ivec2& outPosition) override
  {
    return glfw::createWindow(_mirrorSize);
  }

  void initGl() override
  {
    GlfwApp::initGl();

    // Disable the v-sync for buffer swap
    glfwSwapInterval(0);

    ovrTextureSwapChainDesc desc = {};
    desc.Type = ovrTexture_2D;
    desc.ArraySize = 1;
    desc.Width = _renderTargetSize.x;
    desc.Height = _renderTargetSize.y;
    desc.MipLevels = 1;
    desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.SampleCount = 1;
    desc.StaticImage = ovrFalse;
    ovrResult result = ovr_CreateTextureSwapChainGL(_session, &desc, &_eyeTexture);
    _sceneLayer.ColorTexture[0] = _eyeTexture;
    if (!OVR_SUCCESS(result))
    {
      FAIL("Failed to create swap textures");
    }

    int length = 0;
    result = ovr_GetTextureSwapChainLength(_session, _eyeTexture, &length);
    if (!OVR_SUCCESS(result) || !length)
    {
      FAIL("Unable to count swap chain textures");
    }
    for (int i = 0; i < length; ++i)
    {
      GLuint chainTexId;
      ovr_GetTextureSwapChainBufferGL(_session, _eyeTexture, i, &chainTexId);
      glBindTexture(GL_TEXTURE_2D, chainTexId);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    // Set up the framebuffer object
    glGenFramebuffers(1, &_fbo);
    glGenRenderbuffers(1, &_depthBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _renderTargetSize.x, _renderTargetSize.y);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    ovrMirrorTextureDesc mirrorDesc;
    memset(&mirrorDesc, 0, sizeof(mirrorDesc));
    mirrorDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
    mirrorDesc.Width = _mirrorSize.x;
    mirrorDesc.Height = _mirrorSize.y;
    if (!OVR_SUCCESS(ovr_CreateMirrorTextureGL(_session, &mirrorDesc, &_mirrorTexture)))
    {
      FAIL("Could not create mirror texture");
    }
    glGenFramebuffers(1, &_mirrorFbo);
  }

  void onKey(int key, int scancode, int action, int mods) override
  {
    if (GLFW_PRESS == action)
      switch (key)
      {
      case GLFW_KEY_R:
        ovr_RecenterTrackingOrigin(_session);
        return;
      }

    GlfwApp::onKey(key, scancode, action, mods);
  }

  void draw() final override
  {
    ovrPosef eyePoses[2];
    ovr_GetEyePoses(_session, frame, true, _viewScaleDesc.HmdToEyePose, eyePoses, &_sceneLayer.SensorSampleTime);

    int curIndex;
    ovr_GetTextureSwapChainCurrentIndex(_session, _eyeTexture, &curIndex);
    GLuint curTexId;
    ovr_GetTextureSwapChainBufferGL(_session, _eyeTexture, curIndex, &curTexId);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (OVR_SUCCESS(ovr_GetInputState(_session, ovrControllerType_Touch, &inputState)))
	{

		//Change viewing mode by pressing A
		if (inputState.Buttons & ovrButton_A) {

			if (!a_hasPressed) {
				a_pressed = (a_pressed + 1) % 4;
				std::cout << a_pressed << std::endl;
				a_hasPressed = true;
			}
		}

		if (a_pressed == 0) {
			ovr::for_each_eye([&](ovrEyeType eye)
			{
				const auto& vp = _sceneLayer.Viewport[eye];
				glViewport(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);
				_sceneLayer.RenderPose[eye] = eyePoses[eye];
				renderScene(_eyeProjections[eye], ovr::toGlm(eyePoses[eye]), eye);
			});
		}

		if (a_pressed == 1) {
			const auto& vp = _sceneLayer.Viewport[0];
			glViewport(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);
			_sceneLayer.RenderPose[0] = eyePoses[0];
			renderScene(_eyeProjections[0], ovr::toGlm(eyePoses[0]), 0);
		}

		if (a_pressed == 2) {
			const auto& vp = _sceneLayer.Viewport[1];
			glViewport(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);
			_sceneLayer.RenderPose[1] = eyePoses[1];
			renderScene(_eyeProjections[1], ovr::toGlm(eyePoses[1]), 1);
		}

		if (a_pressed == 3) {
			ovr::for_each_eye([&](ovrEyeType eye)
			{
				const auto& vp = _sceneLayer.Viewport[eye];
				glViewport(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);
				_sceneLayer.RenderPose[eye] = eyePoses[eye];
				renderScene(_eyeProjections[eye], ovr::toGlm(eyePoses[eye]), 1-eye);
			});
		}

		if (!(inputState.Buttons & ovrButton_A) & a_hasPressed) {
			a_hasPressed = false;
		}

	}

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    ovr_CommitTextureSwapChain(_session, _eyeTexture);
    ovrLayerHeader* headerList = &_sceneLayer.Header;
    ovr_SubmitFrame(_session, frame, &_viewScaleDesc, &headerList, 1);

    GLuint mirrorTextureId;
    ovr_GetMirrorTextureBufferGL(_session, _mirrorTexture, &mirrorTextureId);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _mirrorFbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTextureId, 0);
    glBlitFramebuffer(0, 0, _mirrorSize.x, _mirrorSize.y, 0, _mirrorSize.y, _mirrorSize.x, 0, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  }

  virtual void renderScene(const glm::mat4& projection, const glm::mat4& headPose, const int whichEye) = 0;
};

//////////////////////////////////////////////////////////////////////
//
// The remainder of this code is specific to the scene we want to 
// render.  I use glfw to render an array of cubes, but your 
// application would perform whatever rendering you want
//

#define OGLPLUS_USE_GLCOREARB_H 0
#define OGLPLUS_USE_GLEW 1
#define OGLPLUS_USE_BOOST_CONFIG 0
#define OGLPLUS_NO_SITE_CONFIG 1
#define OGLPLUS_LOW_PROFILE 1

#pragma warning( disable : 4068 4244 4267 4065)
#include <oglplus/config/basic.hpp>
#include <oglplus/config/gl.hpp>
#include <oglplus/all.hpp>
#include <oglplus/interop/glm.hpp>
#include <oglplus/bound/texture.hpp>
#include <oglplus/bound/framebuffer.hpp>
#include <oglplus/bound/renderbuffer.hpp>
#include <oglplus/bound/buffer.hpp>
#include <oglplus/shapes/sphere.hpp>
#include <oglplus/shapes/wrapper.hpp>
#pragma warning( default : 4068 4244 4267 4065)


#include <vector>
#include "shader.h"
#include "Cube.h"

namespace Attribute {
	enum {
		Position = 0,
		TexCoord0 = 1,
		Normal = 2,
		Color = 3,
		TexCoord1 = 4,
		InstanceTransform = 5,
	};
}

static const char * VERTEX_SHADER = R"SHADER(
#version 410 core

uniform mat4 ProjectionMatrix = mat4(1);
uniform mat4 ViewMatrix = mat4(1);
uniform mat4 ModelMatrix = mat4(1);

layout(location = 0) in vec4 Position;
layout(location = 1) in vec3 Normal;

out vec3 vertNormal;

void main(void) {
   vertNormal = Normal;
   gl_Position = ProjectionMatrix * ViewMatrix * ModelMatrix * Position;
}
)SHADER";

static const char * FRAGMENT_SHADER = R"SHADER(
#version 410 core

uniform vec4 color = vec4(1);
in vec3 vertNormal;
out vec4 fragColor;

void main(void) {
    fragColor = color;
}
)SHADER";


using namespace oglplus;
// a class for encapsulating building and rendering an RGB cube

class Scene
{
  // Program
  std::vector<glm::mat4> instance_positions;
  GLuint instanceCount;
  GLuint shaderID;

  std::unique_ptr<TexturedCube> cube;
  std::unique_ptr<Skybox> skybox;
  std::unique_ptr<Skybox> skybox_right;

  const unsigned int GRID_SIZE{5};

  glm::mat4 drawView;

  //for render sphere
  Program prog;
  shapes::Sphere makeSphere;
  shapes::DrawingInstructions sphereInstr;
  shapes::Sphere::IndexArray sphereIndices;
  VertexArray sphere;
  Buffer vertices, normals;

  vec3 center = vec3(0.0f, 0.0f, -0.5f);
  vec3 lowerleft = center - vec3(0.14f) * 2.0f;
  std::vector<vec3> sphereLocs;

public:
	Scene() : sphereInstr(makeSphere.Instructions()), sphereIndices(makeSphere.Indices())
	{

		try {
			// attach the shaders to the program
			prog.AttachShader(
				FragmentShader()
				.Source(GLSLSource(String(FRAGMENT_SHADER)))
				.Compile()
			);
			prog.AttachShader(
				VertexShader()
				.Source(GLSLSource(String(VERTEX_SHADER)))
				.Compile()
			);
			prog.Link();
		}
		catch (ProgramBuildError & err) {
			FAIL((const char*)err.what());
		}

		// link and use it
		prog.Use();

		sphere.Bind();
		vertices.Bind(Buffer::Target::Array);
		{
			std::vector<GLfloat> data;
			GLuint n_per_vertex = makeSphere.Positions(data);
			Buffer::Data(Buffer::Target::Array, data);
			(prog | 0).Setup<GLfloat>(n_per_vertex).Enable();
		}

		normals.Bind(Buffer::Target::Array);
		{
			std::vector<GLfloat> data;
			GLuint n_per_vertex = makeSphere.Positions(data);
			Buffer::Data(Buffer::Target::Array, data);
			(prog | 1).Setup<GLfloat>(n_per_vertex).Enable();
		}

		// Create two cube
		instance_positions.push_back(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -0.3)));
		instance_positions.push_back(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -0.9)));

		instanceCount = instance_positions.size();

		// Shader Program
		shaderID = LoadShaders("skybox.vert", "skybox.frag");

		cube = std::make_unique<TexturedCube>("cube");

		// 10m wide sky box: size doesn't matter though
		skybox = std::make_unique<Skybox>("skybox");
		skybox_right = std::make_unique<Skybox>("skybox_righteye");

		skybox->toWorld = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f));
		skybox_right->toWorld = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f));


	}

	void renderSphere(const mat4 & model, const vec4 & color) {
		prog.Use();
		Uniform<mat4>(prog, "ModelMatrix").Set(model);
		Uniform<vec4>(prog, "color").Set(color);
		sphere.Bind();
		sphereInstr.Draw(sphereIndices);
	}

  void render(const glm::mat4& projection, const glm::mat4& view, const int whichEye, const int x_pressed, const float cubeScale, const int b_pressed, const glm::mat3& rot, const glm::vec4& pos, const vec3 & right)
  {

	  // render cursor
	  mat4 S = glm::scale(vec3(0.07 / 2.0f));
	  mat4 rightCursor = glm::translate(mat4(1), right) * S;


	  vec4 rightCursorCorlor = vec4(0, 0, 1, 0);

	  renderSphere(rightCursor, rightCursorCorlor);

	  if (b_pressed == 0) {
		  //ldrawView = view;
		  //rdrawView = view;
		  drawView = view;
	  }

	  if (b_pressed == 2) {
		  /*
		  if (whichEye == 0) {
			  ldrawView = view;
			  ldrawView[3] = lpos;
		  }

		  if (whichEye == 1) {
			  rdrawView = view;
			  rdrawView[3] = rpos;
		  }*/
		  drawView = view;
		  drawView[3] = pos;
	  }

	  if (b_pressed == 1) {
		  /*
		  if (whichEye == 0) {
			  ldrawView = glm::mat4(lrot);
			  ldrawView[3] = view[3];
		  }
		  if(whichEye == 1) {
			  rdrawView = glm::mat4(rrot);
			  rdrawView[3] = view[3];
		  }*/
		  drawView = glm::mat4(rot);
		  drawView[3] = view[3];
	  }

	  if (b_pressed == 3) {
		  /*
		  if (whichEye == 0) {
			  ldrawView = glm::mat4(lrot);
			  ldrawView[3] = lpos;
		  }
		  if (whichEye == 1) {
			  rdrawView = glm::mat4(rrot);
			  rdrawView[3] = rpos;
		  }*/
		  drawView = glm::mat4(rot);
		  drawView[3] = pos;
	  }

	  //Entire scene in stereo
	  if (x_pressed == 0) {
		  // Render two cubes
		  for (int i = 0; i < instanceCount; i++)
		  {
			  // Scale to 20cm: 200cm * 0.1
			  cube->toWorld = instance_positions[i] * glm::scale(glm::mat4(1.0f), glm::vec3(0.15f + 0.1*cubeScale));
			  if (whichEye == 0) {
				  cube->draw(shaderID, projection, drawView);
			  }

			  if (whichEye == 1) {
				  cube->draw(shaderID, projection, drawView);
			  }
		  }

		  // Render Skybox : remove view translation
		  if (whichEye == 0) {
			  skybox->draw(shaderID, projection, drawView);
		  }
		  if (whichEye == 1) {
			  skybox_right->draw(shaderID, projection, drawView);
		  }
	  }

	  //Stereo skybox only
	  if (x_pressed == 1) {

		  // Render Skybox : remove view translation
		  if (whichEye == 0) {
			  skybox->draw(shaderID, projection, drawView);
		  }
		  if (whichEye == 1) {
			  skybox_right->draw(shaderID, projection, drawView);
		  }
	  }

	  //Mono skybox only
	  if (x_pressed == 2) {
		
		  if (whichEye == 0) {
			  skybox->draw(shaderID, projection, drawView);
		  }
		  if (whichEye == 1) {
			  skybox->draw(shaderID, projection, drawView);
		  }
	  }

  }

};

#include <deque> 

// An example application that renders a simple cube
class ExampleApp : public RiftApp
{
  std::shared_ptr<Scene> scene;

public:

	ovrInputState inputState;
	float cubeScale = 0;
	int x_pressed = 0;
	bool x_hasPressed = false;

	int b_pressed = 0;
	bool b_hasPressed = false;

	bool leftIndexP = false;
	bool rightIndexP = false;

	bool leftThumbP = false;
	bool rightThumbP = false;

	int ldelayRender = 0;
	int rdelayRender = 0;

	glm::mat4 leftCurFrame;
	glm::mat4 rightCurFrame;
	glm::mat4 renderFrame;

	float iodOffset = 0;

	glm::mat3 rotation;
	glm::vec4 position;

	std::deque<glm::mat4> lringBuffer;
	std::deque<glm::mat4> rringBuffer;
	std::deque<glm::vec3> cringBuffer;
	int lagNum = 0;
	int delayNum = 0;

  ExampleApp()
  {
	  for(int i = 0; i< 60; i++) {
		  lringBuffer.push_back(glm::mat4(1.0f));
		  rringBuffer.push_back(glm::mat4(1.0f));
		  cringBuffer.push_back(glm::vec3(1.0f));
	  }
  }

protected:

  void initGl() override
  {
    RiftApp::initGl();
    glClearColor(0.2f, 0.2f, 0.2f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    ovr_RecenterTrackingOrigin(_session);
    scene = std::shared_ptr<Scene>(new Scene());
  }

  void shutdownGl() override
  {
    scene.reset();
  }

  void renderScene(const glm::mat4& projection, const glm::mat4& headPose, const int whichEye) override
  {	  
	  if (whichEye == 0) {
		  lringBuffer.pop_front();
		  lringBuffer.push_back(headPose);
	  }

	  if (whichEye == 1) {
		  rringBuffer.pop_front();
		  rringBuffer.push_back(headPose);
	  }


	  //Rendering cursor
	  double displayMidpointSeconds = ovr_GetPredictedDisplayTime(_session, 0);
	  ovrTrackingState trackState = ovr_GetTrackingState(_session, displayMidpointSeconds, ovrTrue);

	  unsigned int handStatus[2];
	  handStatus[0] = trackState.HandStatusFlags[0];
	  handStatus[1] = trackState.HandStatusFlags[1];

	  ovrPosef handPoses[2];  // These are position and orientation in meters in room coordinates, relative to tracking origin. Right-handed cartesian coordinates.
								// ovrQuatf     Orientation;
								// ovrVector3f  Position;

	  handPoses[1] = trackState.HandPoses[1].ThePose;

	  ovrVector3f handPosition[2];
	  handPosition[1] = handPoses[1].Position;
	  vec3 right;
	  right = vec3(handPosition[1].x, handPosition[1].y, handPosition[1].z);

	  cringBuffer.pop_front();
	  //Store the position of the cursor of the current frame
	  cringBuffer.push_back(right);

	  if (OVR_SUCCESS(ovr_GetInputState(_session, ovrControllerType_Touch, &inputState)))
	  {
		  if (inputState.Buttons & ovrButton_X) {

			  if (!x_hasPressed) {
				  x_pressed = (x_pressed + 1) % 3; 
				  std::cout << x_pressed << std::endl;
				  x_hasPressed = true;
			  }
		  }

		  if (!(inputState.Buttons & ovrButton_X) & x_hasPressed) {
			  x_hasPressed = false;
		  }

		  if (inputState.Thumbstick[ovrHand_Left].x) {
			  
			  cubeScale = cubeScale + inputState.Thumbstick[ovrHand_Left].x/10.0f;

			  if (cubeScale > 1.0f) {
				  cubeScale = 1.0f;
			  }

			  if (cubeScale < -1.0f) {
				  cubeScale = -1.0f;
			  }
		  }
		  
		  if (inputState.Buttons & ovrButton_LThumb) {
			  cubeScale = 0;
		  }

		  if (inputState.Buttons & ovrButton_B) {

			  if (!b_hasPressed) {
				  b_pressed = (b_pressed + 1) % 4;
				  std::cout << b_pressed << std::endl;

					glm::mat4 invHeadPose = glm::inverse(headPose);
					rotation = glm::mat3(invHeadPose);
					position = invHeadPose[3];
					b_hasPressed = true;
			  }
		  }

		  if (!(inputState.Buttons & ovrButton_B) & b_hasPressed) {
			  b_hasPressed = false;
		  }

		  if (inputState.Thumbstick[ovrHand_Right].x) {
			  iodOffset = iodOffset + inputState.Thumbstick[ovrHand_Right].x / 100.0f;
			  /*
			  if ((iodOffset + iod) < -0.1f) {

			  }*/
			  setIOD(iodOffset);
		  }

		  if (inputState.Buttons & ovrButton_RThumb) {
			  iodOffset = 0.0f;
			  setIOD(iodOffset);
		  }

		  if (inputState.IndexTrigger[0] > 0.5f) {

			  if (!leftIndexP & lagNum > 0) {
				  lagNum = (lagNum - 1) % 60;
				  std::cout << "Tracking lag: " << lagNum << " frames" << std::endl;
				  leftIndexP = true;
			  }
		  }

		  if (inputState.IndexTrigger[0]<= 0.5f && leftIndexP) {
			  leftIndexP = false;
		  }

		  if (inputState.IndexTrigger[1] > 0.5f) {

			  if (!rightIndexP) {
				  lagNum = (lagNum + 1) % 60;
				  std::cout << "Tracking lag: " << lagNum << " frames" << std::endl;
				  rightIndexP = true;
			  }
		  }

		  if (inputState.IndexTrigger[1] <=0.5f && rightIndexP) {
			  rightIndexP = false;
		  }

		  if (inputState.HandTrigger[0] > 0.5f) {
			  
			  if (!leftThumbP && delayNum > 0) {
				  delayNum = delayNum -1;
				  std::cout << "Rendering delay : " << delayNum << " frames" << std::endl;
				  leftThumbP = true;
			  }

			  if (delayNum < 0) {
				  delayNum = 0;
			  }
		  }

		  if (inputState.HandTrigger[0] <= 0.5f && leftThumbP) {
			  leftThumbP = false;
		  }

		  if (inputState.HandTrigger[1] > 0.5f) {

			  if (!rightThumbP) {
				  delayNum = delayNum + 1;
				  std::cout << "Rendering delay : " << delayNum << " frames" << std::endl;
				  rightThumbP = true;
			  }

			  if (delayNum >= 10) {
				  delayNum = 10;
			  }
		  }

		  if (inputState.HandTrigger[1] <= 0.5f && rightThumbP) {
			  rightThumbP = false;
		  }
	  }

	  glm::mat4 lagFrame = headPose;
	  glm::mat4 outputFrame = headPose;
	  if (whichEye == 0) {
		  lagFrame = lringBuffer[60-lagNum-1];
	  }

	  if (whichEye == 1) {
		  lagFrame = rringBuffer[60-lagNum-1];
	  }

	  right = cringBuffer[60-lagNum-1];

	  if (ldelayRender == 0) {
		  if (whichEye == 0) {
			  leftCurFrame = headPose;
			  renderFrame = leftCurFrame;
		  }
	  }

	  if(rdelayRender == 0){

		  if (whichEye == 1) {
			  rightCurFrame = headPose;
			  renderFrame = rightCurFrame;
		  }
	  }

	  if (ldelayRender < delayNum) {
		  if (whichEye == 0) {
			  renderFrame = leftCurFrame;
			  ldelayRender++;
		  }
	  }

	  if (rdelayRender < delayNum) {
		  if (whichEye == 1) {
			  renderFrame = rightCurFrame;
			  rdelayRender++;
		  }
		  //std::cout << "delayRender is " << delayRender << std::endl;
	  }

	  if (ldelayRender >= delayNum) {
		  ldelayRender = 0;
	  }

	  if (rdelayRender >= delayNum) {
		  rdelayRender = 0;
	  }

	  if (lagNum > 0) {
		  outputFrame = lagFrame;
	  }
	  if (delayNum > 0) {
		  outputFrame = renderFrame;
	  }

	  scene->render(projection, glm::inverse(outputFrame), whichEye, x_pressed, cubeScale, b_pressed, rotation, position, right);
  }
};


// Execute our example class
int main(int argc, char** argv)
{
	int result = -1;

	if (!OVR_SUCCESS(ovr_Initialize(nullptr)))
	{
		FAIL("Failed to initialize the Oculus SDK");
	}
	result = ExampleApp().run();

	//ovr_Shutdown();
	return result;
}
