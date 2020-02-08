#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "agpu.h"
#include "shader.h"

#define UP_DIV(x, y) (((x) + (y) - (1)) / (y))
#define ROUND_UP(x, y) (((x) + (y) - (1)) / (y) * (y))
#define ALIGN_UP4(x) ROUND_UP((x), 4)

namespace agpu {

void agpu_print(const char* m, const float* t, uint32_t rank, uint32_t* dims) {
  static const char* kFloatFormat = "%8.1f";
  std::cout << m;

  for (auto i = 0; i < rank; ++i) {
    assert(dims[i] < 16);
  }

  if (rank == 0) {
    std::cout << *t;
  } else if (rank == 1 || rank == 2) {
    char fbuf[12];
    uint32_t rows = rank == 1 ? 1 : dims[0];
    uint32_t cols = rank == 1 ? dims[0] : dims[1];
    for (uint32_t i = 0; i < rows; i++) {
      std::cout << "\n";
      for (uint32_t j = 0; j < cols; j++) {
        sprintf(fbuf, kFloatFormat, t[i * cols + j]);
        std::cout << fbuf << " ";
      }
    }
  } else if (rank == 3) {
    std::cout << " dims:(";
    for (uint32_t i = 0; i < rank; i++) {
      std::cout << dims[i] << " ";
    }
    std::cout << ")";
    uint32_t d0 = dims[0];
    uint32_t d12size = dims[1] * dims[2];
    for (uint32_t i = 0; i < d0; i++) {
      char s[80];
      sprintf(s, "[%d, *, *]", i);
      agpu_print(s, t + i * d12size, 2, dims + 1);
    }
  } else if (rank == 4) {
    std::cout << " dims:(";
    for (uint32_t i = 0; i < rank; i++) {
      std::cout << dims[i] << " ";
    }
    std::cout << ")";
    uint32_t d0 = dims[0];
    uint32_t d1 = dims[1];
    uint32_t d23size = dims[2] * dims[3];
    for (uint32_t i = 0; i < d0; i++) {
      for (uint32_t j = 0; j < d1; j++) {
        char s[80];
        sprintf(s, "[%d, %d, *, *]", i, j);
        agpu_print(s, t + (i * d0 + j) * d23size, 2, dims + 2);
      }
    }
  } else {
    // TODO: support print r > 4
    assert(false);
  }
  std::cout << std::endl;
}

void agpu_print4d(
    const char* m,
    const float* data,
    uint32_t d0,
    uint32_t d1,
    uint32_t d2,
    uint32_t d3) {
  uint32_t dims[4] = {d0, d1, d2, d3};
  agpu_print(m, data, 4, dims);
}

#ifndef __ANDROID__
// not android

void agpu_conv2d(
    const float* input,
    uint32_t input_n,
    uint32_t input_c,
    uint32_t input_h,
    uint32_t input_w,
    const float* weights,
    uint32_t kernel_c,
    uint32_t kernel_h,
    uint32_t kernel_w,
    const float* bias,
    uint32_t stride_h,
    uint32_t stride_w,
    uint32_t input_padding_h,
    uint32_t input_padding_w,
    uint32_t dilation_h,
    uint32_t dilation_w,
    uint32_t groups,
    float* output) {}

void agpu_add2t(
    const float* input0,
    const float* input1,
    uint32_t n,
    uint32_t c,
    uint32_t h,
    uint32_t w,
    float* output) {}

void agpu_threshold(
    const float* input,
    uint32_t n,
    uint32_t c,
    uint32_t h,
    uint32_t w,
    float threshold,
    float value,
    float* output) {}

void agpu_batch_norm(
    const float* input,
    uint32_t n,
    uint32_t c,
    uint32_t h,
    uint32_t w,
    const float* weight,
    const float* bias,
    const float* mean,
    const float* variance,
    const float eps,
    float* output) {}

void agpu_bench() {}
#else

class AGLContext {
 public:
  AGLContext() {
    if (!(eglGetCurrentContext() != EGL_NO_CONTEXT)) {
      display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
      if (display_ == EGL_NO_DISPLAY) {
        APRINT("eglGetDisplay error");
        isCreateError_ = true;
      }
      int majorVersion;
      int minorVersion;
      eglInitialize(display_, &majorVersion, &minorVersion);
      APRINT("GLContext version major:%d minor:%d", majorVersion, minorVersion);
      EGLint numConfigs;
      static const EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                                             EGL_PBUFFER_BIT,
                                             EGL_RENDERABLE_TYPE,
                                             EGL_OPENGL_ES2_BIT,
                                             EGL_RED_SIZE,
                                             8,
                                             EGL_GREEN_SIZE,
                                             8,
                                             EGL_BLUE_SIZE,
                                             8,
                                             EGL_ALPHA_SIZE,
                                             8,
                                             EGL_NONE};

      EGLConfig surfaceConfig;
      if (!eglChooseConfig(
              display_, configAttribs, &surfaceConfig, 1, &numConfigs)) {
        eglMakeCurrent(
            display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
        APRINT("eglChooseConfig error !!!");
        isCreateError_ = true;
      }

      static const EGLint contextAttribs[] = {
          EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
      context_ =
          eglCreateContext(display_, surfaceConfig, NULL, contextAttribs);
      static const EGLint surfaceAttribs[] = {
          EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
      surface_ =
          eglCreatePbufferSurface(display_, surfaceConfig, surfaceAttribs);
      eglMakeCurrent(display_, surface_, surface_, context_);
      eglBindAPI(EGL_OPENGL_ES_API);
      int major;
      glGetIntegerv(GL_MAJOR_VERSION, &major);
      int minor;
      glGetIntegerv(GL_MINOR_VERSION, &minor);
      APRINT(
          "GLContext: GL_MAJOR_VERSION:%d GL_MINOR_VERSION:%d", major, minor);
      APRINT(
          "GLContext: GL_SHADING_LANGUAGE_VERSION:%s",
          (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

      if (major < 3) {
        isCreateError_ = true;
      }
    } else {
      context_ = EGL_NO_CONTEXT;
      APRINT("eglGetCurrentContext() != EGL_NO_CONTEXT");
      isCreateError_ = true;
    }
  }

  ~AGLContext() {
    if (display_ != EGL_NO_DISPLAY) {
      if (context_ != EGL_NO_CONTEXT) {
        eglDestroyContext(display_, context_);
        context_ = EGL_NO_CONTEXT;
      }
      if (surface_ != EGL_NO_SURFACE) {
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
      }
      eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      eglTerminate(display_);
      display_ = EGL_NO_DISPLAY;
    }
    eglReleaseThread();
  }

  bool isCreateError() const {
    return isCreateError_;
  }

 private:
  EGLContext context_;
  EGLDisplay display_;
  EGLSurface surface_;
  bool isCreateError_{false};
};

class AGLSSBuffer {
 public:
  AGLSSBuffer(GLsizeiptr size, GLenum type = GL_SHADER_STORAGE_BUFFER) {
    type_ = type;
    assert(size > 0);
    glGenBuffers(1, &id_);
    AGL_CHECK_ERROR;
    glBindBuffer(type_, id_);
    AGL_CHECK_ERROR;
    assert(id_ > 0);
    glBufferData(type_, size, NULL, GL_DYNAMIC_DRAW);
    AGL_CHECK_ERROR;
    size_ = size;
  }

  ~AGLSSBuffer() {
    glDeleteBuffers(1, &id_);
    AGL_CHECK_ERROR;
  }

  void* map(GLbitfield bufMask) {
    glBindBuffer(type_, id_);
    AGL_CHECK_ERROR;
    auto p = glMapBufferRange(type_, 0, size_, bufMask);
    AGL_CHECK_ERROR;
    return p;
  }

  void unmap() {
    glBindBuffer(type_, id_);
    glUnmapBuffer(type_);
    AGL_CHECK_ERROR;
  }

  GLsizeiptr size() const {
    return size_;
  }

  void bindInProgram(int binding) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, id_);
    AGL_CHECK_ERROR;
  }

  std::unique_ptr<AGLSSBuffer> static from(
      const float* data,
      GLsizeiptr size,
      size_t sizeCopy) {
    auto buffer = std::make_unique<AGLSSBuffer>(size);
    float* bufferDataPtr =
        (float*)(buffer->map(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
    if (!bufferDataPtr) {
      assert(false);
    }
    memset(bufferDataPtr, 0, size);
    memcpy(bufferDataPtr, data, sizeCopy);
    buffer->unmap();
    return buffer;
  }

  std::unique_ptr<AGLSSBuffer> static from(const float* data, GLsizeiptr size) {
    return from(data, size, size);
  }

 private:
  GLuint id_ = 0;
  GLsizeiptr size_;
  GLenum type_;
}; // class AGLSSBuffer

class AGLTexture {
 public:
  AGLTexture(
      int w,
      int h,
      int d,
      GLenum textureFormat,
      GLenum target = GL_TEXTURE_3D,
      bool HWC4 = true) {
    textureFormat_ = textureFormat;
    if (target == GL_TEXTURE_3D) {
      assert(w > 0 && h > 0 && d > 0);
      target_ = target;
      glGenTextures(1, &id_);
      AGL_CHECK_ERROR;
      glBindTexture(target_, id_);
      AGL_CHECK_ERROR;
      glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      AGL_CHECK_ERROR;
      glTexParameteri(target_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      AGL_CHECK_ERROR;
      glTexParameteri(target_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      AGL_CHECK_ERROR;
      glTexParameteri(target_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      AGL_CHECK_ERROR;
      glTexParameteri(target_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
      AGL_CHECK_ERROR;

      int realW = w;
      int realH = h;
      int realD = d;
      if (HWC4) {
        realD = UP_DIV(d, 4);
        realH = h;
        realW = w;
      }
      glTexStorage3D(target_, 1, textureFormat_, realW, realH, realD);
      AGL_CHECK_ERROR;
    } else if (target == GL_TEXTURE_2D) {
      assert(w > 0 && h > 0);
      target_ = target;
      glGenTextures(1, &id_);
      AGL_CHECK_ERROR;
      glBindTexture(target_, id_);
      AGL_CHECK_ERROR;
      glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      AGL_CHECK_ERROR;
      glTexParameteri(target_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      AGL_CHECK_ERROR;
      glTexParameteri(target_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      AGL_CHECK_ERROR;
      glTexParameteri(target_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      AGL_CHECK_ERROR;
      glTexParameteri(target_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
      AGL_CHECK_ERROR;

      int realW = w;
      int realH = h;
      glTexStorage2D(target_, 1, textureFormat_, realW, realH);
      AGL_CHECK_ERROR;
    }
  }

  ~AGLTexture() {
    glDeleteTextures(1, &id_);
    AGL_CHECK_ERROR;
  }
  unsigned int id() const {
    return id_;
  }

  void read(GLuint unit) {
    glBindImageTexture(unit, id_, 0, GL_TRUE, 0, GL_READ_ONLY, textureFormat_);
    AGL_CHECK_ERROR;
  }

  void write(GLuint unit) {
    glBindImageTexture(unit, id_, 0, GL_TRUE, 0, GL_WRITE_ONLY, textureFormat_);
    AGL_CHECK_ERROR;
  }

  void sample(GLuint unit, GLuint texId) {
    glActiveTexture(GL_TEXTURE0 + texId);
    glUniform1i(unit, texId);
    glBindTexture(target_, id_);
    AGL_CHECK_ERROR;
  }

  void bindInProgram(int programTexId, int binding) {
    glActiveTexture(GL_TEXTURE0 + programTexId);
    glUniform1i(binding, programTexId);
    glBindTexture(GL_TEXTURE_3D, id());
    AGL_CHECK_ERROR;
  }

 private:
  unsigned int id_;
  GLenum target_;
  GLenum textureFormat_{GL_RGBA32F};
}; // class AGLTexture

class AGLShader {
 public:
  AGLShader(const std::string& computeShader) {
    shaderId_ = glCreateShader(GL_COMPUTE_SHADER);
    AGL_CHECK_ERROR;
    const char* _ver[1];
    _ver[0] = computeShader.c_str();
    glShaderSource(shaderId_, 1, _ver, NULL);
    AGL_CHECK_ERROR;

    bool res = compileShader(shaderId_);
    assert(res);

    programId_ = glCreateProgram();
    AGL_CHECK_ERROR;
    glAttachShader(programId_, shaderId_);
    AGL_CHECK_ERROR;
    glLinkProgram(programId_);
    AGL_CHECK_ERROR;
    GLint linked;
    glGetProgramiv(programId_, GL_LINK_STATUS, &linked);
    if (!linked) {
      GLsizei len;
      glGetProgramiv(programId_, GL_INFO_LOG_LENGTH, &len);
      if (len <= 0) {
        glGetProgramInfoLog(programId_, 0, &len, NULL);
      }
      if (len > 0) {
        char* buffer = new char[len + 1];
        buffer[len] = '\0';
        glGetProgramInfoLog(programId_, len, NULL, buffer);
        FUNC_PRINT_ALL(buffer, s);
        delete[] buffer;
      }
    }
  }

  ~AGLShader() {
    glDeleteShader(shaderId_);
    glDeleteProgram(programId_);
    AGL_CHECK_ERROR;
  }

  unsigned int getProgramId() const {
    return programId_;
  }

  static std::string getHead(std::string imageFormat) {
    std::ostringstream headOs;
    headOs << "#version 310 es\n";
    headOs << "#define PRECISION mediump\n";
    headOs << "precision PRECISION float;\n";
    headOs << "#define FORMAT " << imageFormat << "\n";
    return headOs.str();
  }

  void useProgram() {
    glUseProgram(programId_);
    AGL_CHECK_ERROR;
  }

  int getAttribLocation(const char* name) const {
    assert(NULL != name && 0 != programId_);
    return glGetAttribLocation(programId_, name);
  }

  int getUniformLocation(const char* name) const {
    assert(NULL != name && 0 != programId_);
    return glGetUniformLocation(programId_, name);
  }

 private:
  bool compileShader(GLuint s) {
    GLint status;
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);
    if (!status) {
      int len;
      glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
      if (0 >= len) {
        glGetShaderInfoLog(s, 0, &len, NULL);
      }
      char* buffer = new char[len + 1];
      glGetShaderInfoLog(s, len, NULL, buffer);
      buffer[len] = 0;
      FUNC_PRINT_ALL(buffer, s);
      delete[] buffer;
      return false;
    }
    return true;
  }

  unsigned int shaderId_ = 0;
  unsigned int programId_ = 0;
}; // class AGLShader

GLenum getTextureFormat() {
  return GL_RGBA32F;
}

std::string getImageFormat() {
  return "rgba32f";
}

std::unique_ptr<AGLShader> getProgramWithPrefix(
    const char* content,
    const std::vector<std::string>& prefix) {
  std::ostringstream tc;
  tc << AGLShader::getHead(getImageFormat());
  for (auto& s : prefix) {
    tc << s << "\n";
  }
  tc << content;

  APRINT(
      "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n%s\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<",
      tc.str().c_str());

  return std::make_unique<AGLShader>(tc.str());
}

std::unique_ptr<AGLShader> getProgram(const char* content) {
  std::ostringstream tc;
  tc << AGLShader::getHead(getImageFormat()) << content;
  return std::make_unique<AGLShader>(tc.str());
}

std::unique_ptr<AGLShader> getProgram(const std::string&, const char* content) {
  return getProgram(content);
}

std::unique_ptr<AGLShader> getProgram(
    const std::string&,
    const char* content,
    const std::vector<std::string>& prefix) {
  return getProgramWithPrefix(content, prefix);
}

void wait() {
#ifdef USE_GL_FINISH
  glFinish();
#else
  glFlush();
#endif
}

void compute(int dim0, int dim1, int dim2) {
  APRINT("compute(%d %d %d)", dim0, dim1, dim2);
  wait();
  glDispatchCompute(dim0, dim1, dim2);
}

void device2host(
    GLuint textureId,
    float* outputData,
    int d0,
    int d1,
    int d2,
    bool outputAlign4) {
  APRINT("device2host(%d %d %d align %d)", d0, d1, d2, outputAlign4);
  wait();
  auto d2_4 = UP_DIV(d2, 4);
  auto size = d2_4 * 4 * d0 * d1 * sizeof(float);
  auto buffer = std::make_unique<AGLSSBuffer>(size);

  auto program = outputAlign4
      ? getProgram(
            "glsl_image_to_nc4hw4_buffer_glsl",
            glsl_image_to_nc4hw4_buffer_glsl)
      : getProgram(
            "glsl_image_to_nchw_buffer_glsl", glsl_image_to_nchw_buffer_glsl);
  program->useProgram();

  glBindImageTexture(
      0, textureId, 0, GL_TRUE, 0, GL_READ_ONLY, getTextureFormat());
  AGL_CHECK_ERROR;
  buffer->bindInProgram(1);

  glUniform1i(2, d0);
  glUniform1i(3, d1);
  AGL_CHECK_ERROR;

  compute(UP_DIV(d0, 8), UP_DIV(d1, 8), d2_4);
  AGL_CHECK_ERROR;

  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  AGL_CHECK_ERROR;

  auto dOutputData = buffer->map(GL_MAP_READ_BIT);
  if (dOutputData != nullptr) {
    if (outputAlign4) {
      ::memcpy(outputData, dOutputData, size);
    } else {
      ::memcpy(outputData, dOutputData, d0 * d1 * d2 * sizeof(float));
    }
  }
  buffer->unmap();
}

void host2device(
    GLuint textureId,
    const float* inputData,
    const int c,
    const int h,
    const int w,
    const bool inputData4Aligned) {
  APRINT("device2host(c %d h %d w %d align %d)", c, h, w, inputData4Aligned);

  const int c_4 = UP_DIV(c, 4);
  GLsizeiptr size = ROUND_UP(c, 4) * w * h * sizeof(float);
  auto buffer = AGLSSBuffer::from(
      inputData, size, inputData4Aligned ? size : c * h * w * sizeof(float));

  auto program = inputData4Aligned
      ? getProgram(
            "glsl_nc4hw4_buffer_to_image_glsl",
            glsl_nc4hw4_buffer_to_image_glsl)
      : getProgram(
            "glsl_nchw_buffer_to_image_glsl", glsl_nchw_buffer_to_image_glsl);
  program->useProgram();

  glBindImageTexture(
      0, textureId, 0, GL_TRUE, 0, GL_WRITE_ONLY, getTextureFormat());
  AGL_CHECK_ERROR;

  buffer->bindInProgram(1);
  glUniform1i(2, w);
  glUniform1i(3, h);
  AGL_CHECK_ERROR;

  compute(UP_DIV(w, 8), UP_DIV(h, 8), c_4);
  AGL_CHECK_ERROR;
}

static std::unique_ptr<AGLContext> glContext;

void initAGLContextOnce() {
  static const int once = []() {
    APRINT("Creating GLContext...");
    glContext = std::make_unique<AGLContext>();
    if (!glContext) {
      APRINT("ERROR Failed to create GLContext");
      assert(false);
    }
    APRINT("GLContext created ok");
    return 0;
  }();
  ((void)once);
}

void addCompGroupSizeDefines(
    std::vector<std::string>& header,
    int* compGroupSize,
    int compGroupSizeX,
    int compGroupSizeY,
    int compGroupSizeZ) {
  GLint maxCompGroupSizeX, maxCompGroupSizeY, maxCompGroupSizeZ;
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxCompGroupSizeX);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxCompGroupSizeY);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxCompGroupSizeZ);

  compGroupSize[0] =
      compGroupSizeX < maxCompGroupSizeX ? compGroupSizeX : maxCompGroupSizeX;
  compGroupSize[1] =
      compGroupSizeY < maxCompGroupSizeY ? compGroupSizeY : maxCompGroupSizeY;
  compGroupSize[2] =
      compGroupSizeZ < maxCompGroupSizeZ ? compGroupSizeZ : maxCompGroupSizeZ;
  {
    std::ostringstream os;
    os << "#define WORKGROUP_X " << compGroupSize[0];
    header.push_back(os.str());
  }
  {
    std::ostringstream os;
    os << "#define WORKGROUP_Y " << compGroupSize[1];
    header.push_back(os.str());
  }
  {
    std::ostringstream os;
    os << "#define WORKGROUP_Z " << compGroupSize[2];
    header.push_back(os.str());
  }
  APRINT(
      "compGroupSize(%d %d %d)",
      compGroupSize[0],
      compGroupSize[1],
      compGroupSize[2]);
}

void agpu_conv2d(
    const float* input,
    uint32_t input_n,
    uint32_t input_c,
    uint32_t input_h,
    uint32_t input_w,
    const float* weights,
    uint32_t kernel_c,
    uint32_t kernel_h,
    uint32_t kernel_w,
    const float* bias,
    uint32_t stride_h,
    uint32_t stride_w,
    uint32_t input_padding_h,
    uint32_t input_padding_w,
    uint32_t dilation_h,
    uint32_t dilation_w,
    uint32_t groups,
    float* output) {
  APRINT(
      "agpu_conv2d(input nchw %d %d %d %d kernel chw %d %d %d stride hw %d %d i_pad hw %d %d dilation %d %d groups %d",
      input_n,
      input_c,
      input_h,
      input_w,
      kernel_c,
      kernel_h,
      kernel_w,
      stride_h,
      stride_w,
      input_padding_h,
      input_padding_w,
      dilation_h,
      dilation_w,
      groups);
  initAGLContextOnce();

  const int unit = 4;
  const int unit2 = unit * unit;
  const int ic_4 = UP_DIV(input_c, unit);
  const int oc_4 = UP_DIV(kernel_c, unit);

  agpu_print4d("input:", input, input_n, input_c, input_h, input_w);
  agpu_print4d("kernel:", weights, kernel_c, input_c, kernel_h, kernel_w);
  uint32_t bdims[1] = {kernel_c};
  agpu_print("bias:", bias, 1, bdims);

  auto biasBuffer = AGLSSBuffer::from(
      bias, sizeof(float) * ALIGN_UP4(kernel_c), sizeof(float) * kernel_c);

  const uint32_t kernelBufferSize =
      ALIGN_UP4(kernel_c) * ALIGN_UP4(input_c) * kernel_h * kernel_w;
  auto kernelBuffer =
      std::make_unique<AGLSSBuffer>(sizeof(float) * kernelBufferSize);

  const int alignedKernelCSize =
      UP_DIV(input_c, unit) * kernel_w * kernel_h * unit2;
  float* kernelPtr = (float*)(kernelBuffer->map(
      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
  if (kernelPtr) {
    memset(kernelPtr, 0, sizeof(float) * kernelBufferSize);
    const float* src = weights;
    float* dst = kernelPtr;
    int cur = 0;
    // (oc, ic, h, w) -> (oc/4, ic/4, ky kx ic4 oc4)
    for (int b = 0; b < kernel_c; ++b) {
      int b_4 = b / unit;
      float* dst_b = dst + b_4 * alignedKernelCSize;
      int mx = b % unit;
      for (int d = 0; d < input_c; ++d) {
        int my = d % unit;
        int d_4 = d / unit;
        float* dst_d = dst_b + d_4 * kernel_w * kernel_h * unit2;
        for (int y = 0; y < kernel_h; ++y) {
          float* dst_y = dst_d + y * kernel_w * unit2;
          for (int x = 0; x < kernel_w; ++x) {
            float* dst_x = dst_y + x * unit2;
            dst_x[unit * my + mx] = src[cur++];
          }
        }
      }
    }
  }
  kernelBuffer->unmap();

  agpu_print4d("repacked kernel:\n", kernelPtr, kernel_h, kernel_w, unit, unit);

  APRINT("kernelTexture(%d, %d, %d)", ic_4 * unit, oc_4, kernel_w * kernel_h);
  auto kernelTexture = std::make_unique<AGLTexture>(
      ic_4 * unit,
      oc_4,
      kernel_w * kernel_h,
      getTextureFormat(),
      GL_TEXTURE_3D,
      false);

  auto kernel2ImageProgram = getProgram(
      "glsl_kernel2image_adreno_glsl", glsl_kernel2image_adreno_glsl);
  kernel2ImageProgram->useProgram();
  // binding kernel2Image {
  glBindImageTexture(
      0, kernelTexture->id(), 0, GL_TRUE, 0, GL_WRITE_ONLY, getTextureFormat());
  kernelBuffer->bindInProgram(2);
  glUniform1i(3, kernel_w * kernel_h);
  glUniform1i(4, ic_4);
  AGL_CHECK_ERROR;
  // binding kernel2Image }

  compute(ic_4, oc_4, kernel_w * kernel_h);
  AGL_CHECK_ERROR;
  // kernelTexture done

  auto inputTexture = std::make_unique<AGLTexture>(
      input_w, input_h, ic_4, getTextureFormat(), GL_TEXTURE_3D, false);
  host2device(
      inputTexture->id(),
      input,
      input_c,
      input_h,
      input_w,
      false /* input not aligned */);

  int compGroupSize[3];
  std::vector<std::string> header;
  addCompGroupSizeDefines(header, compGroupSize, 1, 1, oc_4);

  auto convProgram = getProgram("convolution", glsl_convolution_glsl, header);

  const uint32_t output_w =
      ((input_w - kernel_w + input_padding_w) / stride_w) + 1;
  const uint32_t output_h =
      ((input_h - kernel_h + input_padding_h) / stride_h) + 1;
  auto outputTexture = std::make_unique<AGLTexture>(
      output_w, output_h, oc_4, getTextureFormat(), GL_TEXTURE_3D, false);

  convProgram->useProgram();

  auto locInput = convProgram->getUniformLocation("uInput");
  auto locKernel = convProgram->getUniformLocation("uKernel");
  auto locPad = convProgram->getUniformLocation("uPad");
  auto locKernelSize = convProgram->getUniformLocation("uKernelSize");
  auto locUnroll = convProgram->getUniformLocation("uUnroll");
  auto locInputSize = convProgram->getUniformLocation("uInputSize");
  APRINT("LLL locInput:%d", locInput);
  APRINT("LLL locKernel:%d", locKernel);
  APRINT("LLL locPad:%d", locPad);
  APRINT("LLL locKernelSize:%d", locKernelSize);
  APRINT("LLL locUnroll:%d", locUnroll);
  APRINT("LLL locInputSize:%d", locInputSize);

  // binding convolution {
  glBindImageTexture(
      0, outputTexture->id(), 0, GL_TRUE, 0, GL_WRITE_ONLY, getTextureFormat());

  inputTexture->bindInProgram(0, 1);
  kernelTexture->bindInProgram(1, 2);
  biasBuffer->bindInProgram(3);
  glUniform2i(4, input_padding_w, input_padding_h);
  glUniform2i(5, kernel_w, kernel_h);
  glUniform2i(6, stride_w, stride_h);
  glUniform2i(7, dilation_w, dilation_h);

  AGL_CHECK_ERROR;
  glUniform3i(10, output_w, output_h, oc_4);
  glUniform3i(11, input_w, input_h, ic_4);

  glUniform1i(8, unit);
  AGL_CHECK_ERROR;
  // binding convolution }

  compute(
      UP_DIV(output_w, unit * compGroupSize[0]),
      UP_DIV(output_h, compGroupSize[1]),
      UP_DIV(oc_4, compGroupSize[2]));
  AGL_CHECK_ERROR;

  device2host(
      outputTexture->id(),
      output,
      output_w,
      output_h,
      kernel_c,
      false /* align */);
  agpu_print4d("conv2d output:", output, input_n, kernel_c, output_h, output_w);
}

void agpu_add2t(
    const float* input0,
    const float* input1,
    uint32_t n,
    uint32_t c,
    uint32_t h,
    uint32_t w,
    float* output) {
  agpu_print4d("agpu_add2t input0:\n", input0, n, c, h, w);
  agpu_print4d("agpu_add2t input1:\n", input1, n, c, h, w);

  initAGLContextOnce();

  int c_4 = UP_DIV(c, 4);

  auto input0Texture = std::make_unique<AGLTexture>(
      w, h, c_4, getTextureFormat(), GL_TEXTURE_3D, false);
  host2device(input0Texture->id(), input0, c, h, w, false /* align */);
  auto input1Texture = std::make_unique<AGLTexture>(
      w, h, c_4, getTextureFormat(), GL_TEXTURE_3D, false);
  host2device(input1Texture->id(), input1, c, h, w, false /* align */);

  auto outputTexture = std::make_unique<AGLTexture>(
      w, h, c_4, getTextureFormat(), GL_TEXTURE_3D, false);

  int compGroupSize[3];
  std::vector<std::string> prefix;
  addCompGroupSizeDefines(prefix, compGroupSize, 8, 8, 1);

  auto binAddProgram =
      getProgram("glsl_binary_add_glsl", glsl_binary_add_glsl, prefix);
  binAddProgram->useProgram();
  // binding glsl_binary_add_glsl {
  glBindImageTexture(
      0, outputTexture->id(), 0, GL_TRUE, 0, GL_WRITE_ONLY, getTextureFormat());
  input0Texture->bindInProgram(0, 1);
  input1Texture->bindInProgram(1, 2);
  glUniform4i(3, w, h, c_4, 1);
  AGL_CHECK_ERROR;
  // binding glsl_binary_add_glsl }

  compute(
      UP_DIV(w, compGroupSize[0]),
      UP_DIV(h, compGroupSize[1]),
      UP_DIV(c_4, compGroupSize[2]));
  device2host(outputTexture->id(), output, w, h, c, false /* align */);

  agpu_print4d("agpu_add2t output:\n", output, n, c, h, w);
}

void agpu_threshold(
    const float* input,
    uint32_t n,
    uint32_t c,
    uint32_t h,
    uint32_t w,
    float threshold,
    float value,
    float* output) {
  agpu_print4d("agpu_threshold input:\n", input, n, c, h, w);

  initAGLContextOnce();

  const int c_4 = UP_DIV(c, 4);
  auto inputTexture = std::make_unique<AGLTexture>(
      w, h, c_4, getTextureFormat(), GL_TEXTURE_3D, false);

  host2device(inputTexture->id(), input, c, h, w, false /* align */);

  auto outputTexture = std::make_unique<AGLTexture>(
      w, h, c_4, getTextureFormat(), GL_TEXTURE_3D, false);

  int compGroupSize[3];
  std::vector<std::string> prefix;
  addCompGroupSizeDefines(prefix, compGroupSize, 8, 8, 1);

  auto program = getProgram("glsl_threshold_glsl", glsl_threshold_glsl, prefix);
  program->useProgram();
  // binding {
  glBindImageTexture(
      0, outputTexture->id(), 0, GL_TRUE, 0, GL_WRITE_ONLY, getTextureFormat());
  inputTexture->bindInProgram(0, 1);

  glUniform4i(2, w, h, c_4, 1);
  glUniform1f(3, threshold);
  glUniform1f(4, value);
  AGL_CHECK_ERROR;
  // binding }
  compute(
      UP_DIV(w, compGroupSize[0]),
      UP_DIV(h, compGroupSize[1]),
      UP_DIV(c_4, compGroupSize[2]));

  device2host(outputTexture->id(), output, w, h, c, false /* align */);

  agpu_print4d("agpu_threshold output:\n", input, n, c, h, w);
}

void agpu_batch_norm(
    const float* input,
    uint32_t n,
    uint32_t c,
    uint32_t h,
    uint32_t w,
    const float* weight,
    const float* bias,
    const float* mean,
    const float* variance,
    const float eps,
    float* output) {
  agpu_print4d("agpu_batch_norm input:\n", input, n, c, h, w);
  initAGLContextOnce();

  int c_4 = UP_DIV(c, 4);

  auto inputTexture = std::make_unique<AGLTexture>(
      w, h, c_4, getTextureFormat(), GL_TEXTURE_3D, false);
  host2device(inputTexture->id(), input, c, h, w, false /* align */);

  auto outputTexture = std::make_unique<AGLTexture>(
      w, h, c_4, getTextureFormat(), GL_TEXTURE_3D, false);

  GLsizeiptr bufferSize = sizeof(float) * ALIGN_UP4(c);
  auto weightBuffer = AGLSSBuffer::from(weight, bufferSize);
  auto biasBuffer = AGLSSBuffer::from(bias, bufferSize);
  auto meanBuffer = AGLSSBuffer::from(mean, bufferSize);
  auto varianceBuffer = AGLSSBuffer::from(variance, bufferSize);

  // computation work group
  int compGroupSize[3];
  std::vector<std::string> prefix;
  addCompGroupSizeDefines(prefix, compGroupSize, 8, 8, 1);

  auto program =
      getProgram("glsl_normalization_glsl", glsl_normalization_glsl, prefix);
  program->useProgram();

  // binding glsl_normalization_glsl {
  glBindImageTexture(
      0, outputTexture->id(), 0, GL_TRUE, 0, GL_WRITE_ONLY, getTextureFormat());

  inputTexture->bindInProgram(0, 1 /* binding */);
  weightBuffer->bindInProgram(3);
  biasBuffer->bindInProgram(4);
  meanBuffer->bindInProgram(5);
  varianceBuffer->bindInProgram(6);

  glUniform1f(7, eps);
  AGL_CHECK_ERROR;
  // binding glsl_normalization_glsl }

  compute(
      UP_DIV(w, compGroupSize[0]),
      UP_DIV(h, compGroupSize[1]),
      UP_DIV(c_4, compGroupSize[2]));

  device2host(outputTexture->id(), output, w, h, c, false /* align */);
  agpu_print4d("agpu_batch_norm output:\n", output, n, c, h, w);
}

#endif
} // namespace agpu