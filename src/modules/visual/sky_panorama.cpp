#include "sky_panorama.hpp"
#include "sky_bindings.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/client_context.hpp"
#include "../core/module_registry.hpp"
#include "../../../resource.h"
#include <Windows.h>
#include <jvmti.h>
#include <gl/GL.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <atomic>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

extern JavaVM* jvm;

namespace
{
    using Microsoft::WRL::ComPtr;
    constexpr GLenum texture0=0x84C0,current_program=0x8B8D,vertex_array_binding=0x85B5;
    constexpr GLenum active_texture=0x84E0,unpack_buffer=0x88EC,unpack_binding=0x88EF;
    constexpr GLenum blend_src_rgb=0x80C9,blend_dst_rgb=0x80C8,blend_src_alpha=0x80CB,blend_dst_alpha=0x80CA;
    constexpr GLenum equation_rgb=0x8009,equation_alpha=0x883D,func_add=0x8006;
    struct gl_api
    {
#define SKY_FUNCTION(name,result,...) using name##_fn=result(APIENTRY*)(__VA_ARGS__); name##_fn name=nullptr;
        SKY_FUNCTION(ActiveTexture,void,GLenum)
        SKY_FUNCTION(CreateShader,GLuint,GLenum)
        SKY_FUNCTION(ShaderSource,void,GLuint,GLsizei,const char* const*,const GLint*)
        SKY_FUNCTION(CompileShader,void,GLuint)
        SKY_FUNCTION(GetShaderiv,void,GLuint,GLenum,GLint*)
        SKY_FUNCTION(DeleteShader,void,GLuint)
        SKY_FUNCTION(CreateProgram,GLuint,void)
        SKY_FUNCTION(AttachShader,void,GLuint,GLuint)
        SKY_FUNCTION(LinkProgram,void,GLuint)
        SKY_FUNCTION(GetProgramiv,void,GLuint,GLenum,GLint*)
        SKY_FUNCTION(DeleteProgram,void,GLuint)
        SKY_FUNCTION(UseProgram,void,GLuint)
        SKY_FUNCTION(GenVertexArrays,void,GLsizei,GLuint*)
        SKY_FUNCTION(BindVertexArray,void,GLuint)
        SKY_FUNCTION(DeleteVertexArrays,void,GLsizei,const GLuint*)
        SKY_FUNCTION(GetUniformLocation,GLint,GLuint,const char*)
        SKY_FUNCTION(Uniform1i,void,GLint,GLint)
        SKY_FUNCTION(Uniform1f,void,GLint,GLfloat)
        SKY_FUNCTION(Uniform3f,void,GLint,GLfloat,GLfloat,GLfloat)
        SKY_FUNCTION(UniformMatrix4fv,void,GLint,GLsizei,GLboolean,const GLfloat*)
        SKY_FUNCTION(GenerateMipmap,void,GLenum)
        SKY_FUNCTION(BindBuffer,void,GLenum,GLuint)
        SKY_FUNCTION(BlendFuncSeparate,void,GLenum,GLenum,GLenum,GLenum)
        SKY_FUNCTION(BlendEquationSeparate,void,GLenum,GLenum)
        SKY_FUNCTION(BindSampler,void,GLuint,GLuint)
#undef SKY_FUNCTION
        bool load()
        {
            const auto proc=[](const char* name) {
                auto address=wglGetProcAddress(name);
                const auto value=reinterpret_cast<std::intptr_t>(address);
                return value>3 || value<-1 ? address : nullptr;
            };
#define SKY_LOAD(name) name=reinterpret_cast<name##_fn>(proc("gl" #name)); if (!name) return false;
            SKY_LOAD(ActiveTexture) SKY_LOAD(CreateShader) SKY_LOAD(ShaderSource) SKY_LOAD(CompileShader)
            SKY_LOAD(GetShaderiv) SKY_LOAD(DeleteShader) SKY_LOAD(CreateProgram) SKY_LOAD(AttachShader)
            SKY_LOAD(LinkProgram) SKY_LOAD(GetProgramiv) SKY_LOAD(DeleteProgram) SKY_LOAD(UseProgram)
            SKY_LOAD(GenVertexArrays) SKY_LOAD(BindVertexArray) SKY_LOAD(DeleteVertexArrays)
            SKY_LOAD(GetUniformLocation) SKY_LOAD(Uniform1i) SKY_LOAD(Uniform1f) SKY_LOAD(Uniform3f) SKY_LOAD(UniformMatrix4fv)
            SKY_LOAD(GenerateMipmap) SKY_LOAD(BindBuffer) SKY_LOAD(BlendFuncSeparate) SKY_LOAD(BlendEquationSeparate)
#undef SKY_LOAD
            BindSampler=reinterpret_cast<BindSampler_fn>(proc("glBindSampler"));
            return true;
        }
    };
    struct gl_restore
    {
        gl_api& gl;
        GLint program=0,vao=0,active=0,texture=0,sampler=0,depth_func=0,polygon[2]{};
        GLint src_rgb=0,dst_rgb=0,src_a=0,dst_a=0,eq_rgb=0,eq_a=0;
        GLboolean depth_write{},colors[4]{};
        GLdouble range[2]{};
        const std::array<GLenum,6> capabilities{GL_BLEND,GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,0x8C89};
        std::array<GLboolean,6> enabled{};
        explicit gl_restore(gl_api& api) : gl(api)
        {
            glGetIntegerv(current_program,&program); glGetIntegerv(vertex_array_binding,&vao);
            glGetIntegerv(active_texture,&active); gl.ActiveTexture(texture0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
            if (gl.BindSampler) glGetIntegerv(0x8919,&sampler);
            glGetIntegerv(GL_DEPTH_FUNC,&depth_func); glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_write);
            glGetBooleanv(GL_COLOR_WRITEMASK,colors); glGetDoublev(GL_DEPTH_RANGE,range);
            glGetIntegerv(GL_POLYGON_MODE,polygon);
            glGetIntegerv(blend_src_rgb,&src_rgb); glGetIntegerv(blend_dst_rgb,&dst_rgb);
            glGetIntegerv(blend_src_alpha,&src_a); glGetIntegerv(blend_dst_alpha,&dst_a);
            glGetIntegerv(equation_rgb,&eq_rgb); glGetIntegerv(equation_alpha,&eq_a);
            for (std::size_t i=0;i<capabilities.size();++i) enabled[i]=glIsEnabled(capabilities[i]);
        }
        ~gl_restore()
        {
            gl.UseProgram(program); gl.BindVertexArray(vao); glBindTexture(GL_TEXTURE_2D,texture);
            if (gl.BindSampler) gl.BindSampler(0,sampler);
            gl.ActiveTexture(active); glDepthFunc(depth_func); glDepthMask(depth_write);
            glColorMask(colors[0],colors[1],colors[2],colors[3]); glDepthRange(range[0],range[1]);
            glPolygonMode(GL_FRONT_AND_BACK,polygon[0]);
            gl.BlendFuncSeparate(src_rgb,dst_rgb,src_a,dst_a); gl.BlendEquationSeparate(eq_rgb,eq_a);
            for (std::size_t i=0;i<capabilities.size();++i)
                if (enabled[i]) glEnable(capabilities[i]); else glDisable(capabilities[i]);
        }
    };
    std::span<const unsigned char> resource(int id)
    {
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&resource),&module);
        const auto entry=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
        if (!entry) return {};
        const auto data=LoadResource(module,entry);
        return {static_cast<const unsigned char*>(LockResource(data)),SizeofResource(module,entry)};
    }
    GLuint load_texture(gl_api& gl,int id)
    {
        const auto bytes=resource(id);
        if (bytes.empty()) return 0;
        const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        struct com_restore { HRESULT result; ~com_restore() { if (SUCCEEDED(result)) CoUninitialize(); } } undo_com{com};
        ComPtr<IWICImagingFactory> factory; ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapDecoder> decoder; ComPtr<IWICBitmapFrameDecode> frame;
        ComPtr<IWICFormatConverter> converter;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))) ||
            FAILED(factory->CreateStream(&stream)) ||
            FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),static_cast<DWORD>(bytes.size()))) ||
            FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder)) ||
            FAILED(decoder->GetFrame(0,&frame)) || FAILED(factory->CreateFormatConverter(&converter)) ||
            FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))) return 0;
        UINT width=0,height=0;
        if (FAILED(frame->GetSize(&width,&height)) || !width || !height || width>8192 || height>4096) return 0;
        GLint maximum=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
        if (width>static_cast<UINT>(maximum) || height>static_cast<UINT>(maximum)) return 0;
        std::vector<BYTE> pixels(static_cast<std::size_t>(width)*height*4);
        if (FAILED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data()))) return 0;
        struct unpack_restore
        {
            gl_api& gl; GLint buffer=0,alignment=0,rows=0,skip_rows=0,skip_pixels=0;
            explicit unpack_restore(gl_api& g) : gl(g)
            {
                glGetIntegerv(unpack_binding,&buffer); glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
                glGetIntegerv(GL_UNPACK_ROW_LENGTH,&rows); glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skip_rows);
                glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skip_pixels);
                gl.BindBuffer(unpack_buffer,0); glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH,0); glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
            }
            ~unpack_restore()
            {
                gl.BindBuffer(unpack_buffer,buffer); glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);
                glPixelStorei(GL_UNPACK_ROW_LENGTH,rows); glPixelStorei(GL_UNPACK_SKIP_ROWS,skip_rows);
                glPixelStorei(GL_UNPACK_SKIP_PIXELS,skip_pixels);
            }
        } undo_unpack(gl);
        GLuint texture=0; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        GLint uploaded=0; glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&uploaded);
        if (uploaded!=static_cast<GLint>(width)) { glDeleteTextures(1,&texture); return 0; }
        gl.GenerateMipmap(GL_TEXTURE_2D);
        return texture;
    }
    constexpr const char* vertex_source=R"(#version 150
out vec2 screen;
void main() {
    vec2 p=vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    screen=p*2.0-1.0;
    gl_Position=vec4(screen,1.0,1.0);
})";
    constexpr const char* fragment_source=R"(#version 150
uniform sampler2D Panorama;
uniform mat4 InverseViewProjection;
uniform float Rotation, Exposure, Opacity, SkyCrop, Stylized, FogVisibility, HazeHeight;
uniform vec3 FogColor;
in vec2 screen;
out vec4 color;
void main() {
    vec3 direction=normalize((InverseViewProjection*vec4(screen,1.0,1.0)).xyz);
    // atan has an undefined value at the pole and a periodic jump at the wrap.
    float poleGuard=abs(direction.x)+abs(direction.z)<0.000001 ? 0.000001 : 0.0;
    float u=fract(atan(direction.z,direction.x+poleGuard)*0.159154943+0.5+Rotation);
    float v=acos(clamp(direction.y,0.0,1.0))*0.318309886*SkyCrop;
    vec2 uv=vec2(u,v);
    // Explicit, wrapped gradients prevent the 0/1 jump selecting an extremely
    // blurry mip level. Compute them before any non-uniform seam branches.
    vec2 dx=dFdx(uv),dy=dFdy(uv);
    dx.x-=floor(dx.x+0.5); dy.x-=floor(dy.x+0.5);
    vec3 image=textureGrad(Panorama,uv,dx,dy).rgb;
    if (Stylized>0.5) {
        // Cross-fade the wrap seam and converge at the zenith; generated artwork
        // is not guaranteed to have perfectly periodic edges or a uniform pole.
        float edge=min(u,1.0-u);
        if (edge<0.025) {
            vec3 opposite=textureGrad(Panorama,vec2(1.0-u,v),vec2(-dx.x,dx.y),vec2(-dy.x,dy.y)).rgb;
            image=mix(opposite,image,0.5+0.5*smoothstep(0.0,0.025,edge));
        }
        vec3 pole=textureLod(Panorama,vec2(0.5,0.008),5.0).rgb;
        image=mix(image,pole,smoothstep(0.985,0.9995,direction.y));
    }
    // Below the horizon use fog instead of stretching a repeated texture row.
    // Exposure affects the artwork, not the game's fog color at the terrain edge.
    vec3 exposed=clamp(image*exp2(Exposure),0.0,1.0);
    float detail=smoothstep(0.0,HazeHeight,direction.y)*FogVisibility;
    color=vec4(mix(FogColor,exposed,detail),Opacity);
})";
}

struct sky_panorama::renderer::runtime
{
    inline static runtime* current=nullptr;
    gl_api gl;
    HGLRC owner=nullptr;
    GLuint program=0,vao=0;
    std::array<GLuint,6> textures{};
    jclass bridge=nullptr;
    jmethodID stop=nullptr,shader_check=nullptr,healthy=nullptr,color_configure=nullptr;
    jvmtiEnv* tooling=nullptr;
    jclass fog_class=nullptr,transformer=nullptr,fog_bridge=nullptr;
    jmethodID transform=nullptr,fog_configure=nullptr,fog_stop=nullptr;
    inline static std::atomic<runtime*> transforming{nullptr};
    bool fog_changed=false;
    module_settings::sky config;
    bool enabled=false,reduced=false,failed=false;
    std::string failure;
    double phase=0,last_time=0;

    static jboolean JNICALL read_uniform(JNIEnv* env,jclass,jint buffer,jlong offset,jlong available,jfloatArray values)
    {
        if (!wglGetCurrentContext() || buffer<=0 || offset<0 || available<0 || !values) return JNI_FALSE;
        const auto count=env->GetArrayLength(values);
        if (count<1 || count>16 || available<static_cast<jlong>(count*sizeof(float))) return JNI_FALSE;
        using bind_fn=void(APIENTRY*)(GLenum,GLuint);
        using size_fn=void(APIENTRY*)(GLenum,GLenum,GLint*);
        using read_fn=void(APIENTRY*)(GLenum,std::intptr_t,std::intptr_t,void*);
        const auto bind=reinterpret_cast<bind_fn>(wglGetProcAddress("glBindBuffer"));
        const auto size=reinterpret_cast<size_fn>(wglGetProcAddress("glGetBufferParameteriv"));
        const auto read=reinterpret_cast<read_fn>(wglGetProcAddress("glGetBufferSubData"));
        const auto valid=[](auto function) { const auto address=reinterpret_cast<std::intptr_t>(function); return address>3 || address<-1; };
        if (!valid(bind) || !valid(size) || !valid(read)) return JNI_FALSE;
        constexpr GLenum copy_read=0x8F36,buffer_size=0x8764;
        GLint previous=0,capacity=0; glGetIntegerv(copy_read,&previous);
        bind(copy_read,static_cast<GLuint>(buffer));
        struct restore { bind_fn bind; GLint previous; ~restore() { bind(0x8F36,previous); } } guard{bind,previous};
        size(copy_read,buffer_size,&capacity);
        const jlong bytes=count*sizeof(float);
        if (offset>capacity || bytes>capacity-offset) return JNI_FALSE;
        std::array<float,16> data{};
        read(copy_read,static_cast<std::intptr_t>(offset),static_cast<std::intptr_t>(bytes),data.data());
        for (int index=0;index<count;++index) if (!std::isfinite(data[index])) return JNI_FALSE;
        env->SetFloatArrayRegion(values,0,count,data.data());
        return env->ExceptionCheck() ? JNI_FALSE : JNI_TRUE;
    }

    static void JNICALL fog_bytes(jvmtiEnv* tools,JNIEnv* env,jclass redefining,jobject,
        const char* name,jobject,jint length,const unsigned char* bytes,jint* output_length,unsigned char** output)
    {
        auto* self=transforming.load(std::memory_order_acquire);
        if (!self || !redefining || !name || std::strcmp(name,ONESTEP_MC_NAME("net/minecraft/class_758"))!=0 ||
            !env->IsSameObject(redefining,self->fog_class)) return;
        if (env->PushLocalFrame(8)!=JNI_OK) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }
        auto original=env->NewByteArray(length);
        if (original) env->SetByteArrayRegion(original,0,length,reinterpret_cast<const jbyte*>(bytes));
        auto result=original && !env->ExceptionCheck()
            ? static_cast<jbyteArray>(env->CallStaticObjectMethod(self->transformer,self->transform,original,static_cast<jint>(mc::target.build_id))) : nullptr;
        if (result && !env->ExceptionCheck())
        {
            const auto count=env->GetArrayLength(result);
            unsigned char* copy=nullptr;
            if (count>0 && count<length*4+65536 && tools->Allocate(count,&copy)==JVMTI_ERROR_NONE)
            {
                env->GetByteArrayRegion(result,0,count,reinterpret_cast<jbyte*>(copy));
                if (!env->ExceptionCheck()) { *output_length=count; *output=copy; self->fog_changed=true; }
                else tools->Deallocate(copy);
            }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->PopLocalFrame(nullptr);
    }

    void install_fog(JNIEnv* env,jobject loader,jobject game_loader)
    {
        auto loader_class=env->GetObjectClass(game_loader);
        auto load=env->GetMethodID(loader_class,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");
        auto name=env->NewStringUTF("net.onestep.visual.SkyFogBridge");
        auto fog_type=static_cast<jclass>(env->CallObjectMethod(game_loader,load,name));
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (!fog_type)
        {
            const auto bridge_bytes=resource(IDR_SKY_FOG_BRIDGE);
            if (bridge_bytes.empty()) throw std::runtime_error("Missing fog bridge");
            fog_type=env->DefineClass("net/onestep/visual/SkyFogBridge",game_loader,
                reinterpret_cast<const jbyte*>(bridge_bytes.data()),static_cast<jsize>(bridge_bytes.size()));
        }
        if (!fog_type || env->ExceptionCheck()) throw std::runtime_error("Fog bridge class");
        fog_bridge=static_cast<jclass>(env->NewGlobalRef(fog_type));
        auto prepare=env->GetStaticMethodID(fog_bridge,"prepare","(Ljava/lang/ClassLoader;)V");
        fog_configure=env->GetStaticMethodID(fog_bridge,"configure","(IIFZZ)V");
        fog_stop=env->GetStaticMethodID(fog_bridge,"stop","()V");
        if (!prepare || !fog_configure || !fog_stop || env->ExceptionCheck()) throw std::runtime_error("Fog bridge interface");
        env->CallStaticVoidMethod(fog_bridge,prepare,game_loader);
        if (env->ExceptionCheck()) throw std::runtime_error("Fog bridge initialization");
        const auto bytes=resource(IDR_SKY_TRANSFORMER);
        if (bytes.empty()) throw std::runtime_error("Missing fog transformer");
        auto type=env->DefineClass("net/onestep/visual/SkyTransformer",loader,
            reinterpret_cast<const jbyte*>(bytes.data()),static_cast<jsize>(bytes.size()));
        if (!type || env->ExceptionCheck()) throw std::runtime_error("Fog transformer");
        transformer=static_cast<jclass>(env->NewGlobalRef(type));
        transform=env->GetStaticMethodID(transformer,"transform","([BI)[B");
        auto klass=jni::_custom_find_class ? jni::_custom_find_class(ONESTEP_MC_NAME("net/minecraft/class_758")) : nullptr;
        if (!klass || !transform || env->ExceptionCheck()) throw std::runtime_error("Fog class");
        fog_class=static_cast<jclass>(env->NewGlobalRef(klass));
        if (jvm->GetEnv(reinterpret_cast<void**>(&tooling),JVMTI_VERSION_1_2)!=JNI_OK || !tooling)
            throw std::runtime_error("Fog JVMTI");
        jvmtiCapabilities capabilities{}; capabilities.can_retransform_classes=1;
        if (tooling->AddCapabilities(&capabilities)!=JVMTI_ERROR_NONE) throw std::runtime_error("Fog capabilities");
        jvmtiEventCallbacks callbacks{}; callbacks.ClassFileLoadHook=&fog_bytes;
        if (tooling->SetEventCallbacks(&callbacks,sizeof(callbacks))!=JVMTI_ERROR_NONE) throw std::runtime_error("Fog callback");
        transforming.store(this,std::memory_order_release);
        if (tooling->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr)!=JVMTI_ERROR_NONE ||
            tooling->RetransformClasses(1,&fog_class)!=JVMTI_ERROR_NONE || !fog_changed)
            throw std::runtime_error("Fog transformation");
    }

    static void JNICALL draw(JNIEnv* env,jclass,jfloatArray projection,jfloatArray position,jfloatArray fog)
    {
        struct clear_on_exit
        {
            JNIEnv* env;
            ~clear_on_exit() { if (env->ExceptionCheck()) env->ExceptionClear(); }
        } cleanup{env};
        if (!current || !current->enabled || gui::should_unload || gui::loader_visible) return;
        const auto* module=modules::find_module_by_id("sky");
        if (!module || !module->enabled) return;
        try
        {
            if (!projection || !position || !fog || env->GetArrayLength(projection)!=16 ||
                env->GetArrayLength(position)!=16 || env->GetArrayLength(fog)!=5) return;
            std::array<float,16> p{},v{};
            std::array<float,5> f{};
            env->GetFloatArrayRegion(projection,0,16,p.data()); env->GetFloatArrayRegion(position,0,16,v.data());
            env->GetFloatArrayRegion(fog,0,5,f.data());
            if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
            for (float value:f) if (!std::isfinite(value)) return;
            current->render(p.data(),v.data(),f.data());
        }
        catch (...) { platform::jni_context::clear_exception(); current->failure="Panorama rendering stopped."; current->enabled=false; }
    }
    bool initialize()
    {
        const auto compile=[&](GLenum type,const char* text) {
            const GLuint shader=gl.CreateShader(type);
            if (!shader) return GLuint{0};
            gl.ShaderSource(shader,1,&text,nullptr); gl.CompileShader(shader);
            GLint ok=0; gl.GetShaderiv(shader,0x8B81,&ok);
            if (!ok) { gl.DeleteShader(shader); return GLuint{0}; }
            return shader;
        };
        const GLuint vertex=compile(0x8B31,vertex_source),fragment=compile(0x8B30,fragment_source);
        if (!vertex || !fragment)
        {
            if (vertex) gl.DeleteShader(vertex); if (fragment) gl.DeleteShader(fragment);
            return false;
        }
        program=gl.CreateProgram(); gl.AttachShader(program,vertex); gl.AttachShader(program,fragment);
        gl.LinkProgram(program); gl.DeleteShader(vertex); gl.DeleteShader(fragment);
        GLint ok=0; gl.GetProgramiv(program,0x8B82,&ok);
        if (!ok) { gl.DeleteProgram(program); program=0; return false; }
        gl.GenVertexArrays(1,&vao);
        return vao!=0;
    }
    void render(const float* projection,const float* position,const float* fog)
    {
        if (!platform::jni_context::bind_current_thread()) return;
        jni::frame frame{48};
        services::game::client_context context;
        if (!services::game::capture_client_context(context,true,false)) return;
        sky_bindings::World world(context.world.object_instance,false);
        auto dimension=world.getDimension.call();
        if (platform::jni_context::clear_exception() || !dimension) return;
        const bool skylight=dimension.hasSkyLight.call()==JNI_TRUE;
        if (platform::jni_context::clear_exception() || !skylight) return;
        auto renderer=context.instance.gameRenderer.get();
        if (platform::jni_context::clear_exception() || !renderer) return;
        auto camera=renderer.getCamera.call();
        if (platform::jni_context::clear_exception() || !camera) return;
        sky_bindings::Camera view(camera.object_instance,false);
        auto submerged=view.getSubmersionType.call();
        if (platform::jni_context::clear_exception() || !submerged) return;
        auto air=sky_bindings::Submersion{}.NONE.get();
        if (platform::jni_context::clear_exception() || !air || !air.is_same_object(submerged)) return;
        if (platform::jni_context::clear_exception()) return;
        const HGLRC context_gl=wglGetCurrentContext();
        if (!context_gl || failed) return;
        if (!owner)
        {
            owner=context_gl;
            if (!gl.load()) { failed=true; failure="Required OpenGL functions for the panorama are unavailable."; return; }
        }
        if (owner!=context_gl) return; // Never use a VAO from another context.
        GLdouble clear_depth=0; glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clear_depth);
        if (clear_depth<.99999) return;
        auto view_matrix=glm::make_mat4(position); view_matrix[3]=glm::vec4(0,0,0,1);
        const auto combined=glm::make_mat4(projection)*view_matrix;
        const auto determinant=glm::determinant(combined);
        if (!std::isfinite(determinant) || std::abs(determinant)<1e-8f) return;
        const auto inverse=glm::inverse(combined);
        for (int i=0;i<16;++i) if (!std::isfinite(glm::value_ptr(inverse)[i])) return;
        gl_restore restore(gl);
        if (!program && !initialize()) { failed=true; failure="Could not initialize the panorama shader."; return; }
        const int selected=std::clamp(config.panorama,0,5);
        constexpr int resources[]{IDR_SKY_NIGHT,IDR_SKY_MILKY_WAY,IDR_SKY_SUNRISE,
            IDR_SKY_CANDY_DUSK,IDR_SKY_NEBULA,IDR_SKY_AURORA};
        if (!textures[selected]) textures[selected]=load_texture(gl,resources[selected]);
        if (!textures[selected]) { failed=true; failure="Could not load the panorama image."; return; }
        const double now=static_cast<double>(GetTickCount64())/1000.;
        const double dt=last_time>0 ? std::clamp(now-last_time,0.,.1) : 0.; last_time=now;
        if (config.drift && !reduced) phase=std::fmod(phase+dt*.00055,1.);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE); glDepthRange(0,1);
        glEnable(GL_BLEND); gl.BlendEquationSeparate(func_add,func_add);
        gl.BlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST); glDisable(0x8C89);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        gl.UseProgram(program); gl.BindVertexArray(vao); glBindTexture(GL_TEXTURE_2D,textures[selected]);
        if (gl.BindSampler) gl.BindSampler(0,0);
        gl.Uniform1i(gl.GetUniformLocation(program,"Panorama"),0);
        gl.UniformMatrix4fv(gl.GetUniformLocation(program,"InverseViewProjection"),1,GL_FALSE,glm::value_ptr(inverse));
        gl.Uniform1f(gl.GetUniformLocation(program,"Rotation"),config.rotation/360.f+static_cast<float>(phase));
        gl.Uniform1f(gl.GetUniformLocation(program,"Exposure"),config.exposure);
        gl.Uniform1f(gl.GetUniformLocation(program,"Opacity"),config.strength);
        // The generated skies place their horizon below the equator. Extend the
        // sample range a little rather than stretching clouds into the zenith.
        constexpr float crops[]{.98f,.68f,.98f,1.12f,1.16f,1.24f};
        gl.Uniform1f(gl.GetUniformLocation(program,"SkyCrop"),crops[selected]);
        gl.Uniform1f(gl.GetUniformLocation(program,"Stylized"),selected>=3 ? 1.f : 0.f);
        const float fog_end=std::clamp(fog[4],0.f,65536.f);
        const float visibility=std::clamp((fog_end-16.f)/48.f,0.f,1.f);
        gl.Uniform1f(gl.GetUniformLocation(program,"FogVisibility"),visibility*visibility*(3.f-2.f*visibility));
        const float clear_fraction=std::clamp(fog[3]/std::max(fog_end,.001f),0.f,1.f);
        gl.Uniform1f(gl.GetUniformLocation(program,"HazeHeight"),.14f+.14f*(1.f-clear_fraction));
        gl.Uniform3f(gl.GetUniformLocation(program,"FogColor"),std::clamp(fog[0],0.f,1.f),
            std::clamp(fog[1],0.f,1.f),std::clamp(fog[2],0.f,1.f));
        glDrawArrays(GL_TRIANGLES,0,3);
        failure.clear();
    }
};

sky_panorama::renderer::renderer() : state_(std::make_unique<runtime>()) {}
sky_panorama::renderer::~renderer() = default;
bool sky_panorama::renderer::connect()
{
    auto& s=*state_;
    if (s.bridge) return true;
    if (s.failed || !platform::jni_context::bind_current_thread()) return false;
    JNIEnv* env=jni::get_env();
    jni::frame frame{48};
    try
    {
        const auto checked=[&](auto value) {
            if (env->ExceptionCheck()) { env->ExceptionClear(); throw std::runtime_error("JNI"); }
            if (!value) throw std::runtime_error("JNI");
            return value;
        };
        jclass game=checked(jni::get_cached_jclass<maps::MinecraftClient>());
        auto class_type=checked(env->FindClass("java/lang/Class"));
        auto get_loader=checked(env->GetMethodID(class_type,"getClassLoader","()Ljava/lang/ClassLoader;"));
        auto parent=checked(env->CallObjectMethod(game,get_loader));
        auto loader_type=checked(env->FindClass("java/net/URLClassLoader"));
        auto constructor=checked(env->GetMethodID(loader_type,"<init>","([Ljava/net/URL;Ljava/lang/ClassLoader;)V"));
        auto url_type=checked(env->FindClass("java/net/URL"));
        auto urls=checked(env->NewObjectArray(0,url_type,nullptr));
        auto loader=checked(env->NewObject(loader_type,constructor,urls,parent));
        const auto bytes=resource(IDR_SKY_BRIDGE);
        if (bytes.empty()) throw std::runtime_error("Missing bridge");
        auto bridge=checked(env->DefineClass("net/onestep/visual/SkyBridge",loader,
            reinterpret_cast<const jbyte*>(bytes.data()),static_cast<jsize>(bytes.size())));
        JNINativeMethod methods[]{
            {const_cast<char*>("draw"),const_cast<char*>("([F[F[F)V"),reinterpret_cast<void*>(&runtime::draw)},
            {const_cast<char*>("readUniform"),const_cast<char*>("(IJJ[F)Z"),reinterpret_cast<void*>(&runtime::read_uniform)}
        };
        if (env->RegisterNatives(bridge,methods,2)!=JNI_OK) throw std::runtime_error("Native bridge");
        s.bridge=static_cast<jclass>(checked(env->NewGlobalRef(bridge)));
        s.stop=checked(env->GetStaticMethodID(bridge,"stop","()V"));
        s.shader_check=checked(env->GetStaticMethodID(bridge,"shaderBlocked","()Z"));
        s.healthy=checked(env->GetStaticMethodID(bridge,"healthy","()Z"));
        s.color_configure=checked(env->GetStaticMethodID(bridge,"configureColors","(IIFZZ)V"));
        auto install=checked(env->GetStaticMethodID(bridge,"install","(Ljava/lang/ClassLoader;I)V"));
        runtime::current=&s;
        env->CallStaticVoidMethod(bridge,install,parent,static_cast<jint>(mc::target.build_id));
        if (env->ExceptionCheck()) throw std::runtime_error("Fabric callback");
        s.install_fog(env,loader,parent);
        s.failure.clear(); return true;
    }
    catch (...)
    {
        platform::jni_context::clear_exception(); shutdown();
        s.failed=true; s.failure="Could not connect the panorama bridge; Fabric API is required.";
        return false;
    }
}
void sky_panorama::renderer::configure(const module_settings::sky& config,bool enabled,bool reduced)
{
    state_->config=config; state_->enabled=enabled && !state_->failed; state_->reduced=reduced;
    if (!enabled) state_->last_time=0;
}
void sky_panorama::renderer::configure_colors(int sky,int fog,float strength,bool match_fog,bool enabled)
{
    auto& s=*state_;
    if (!s.bridge || !s.color_configure || !jni::get_env()) return;
    if (enabled)
    {
        const bool alive=s.healthy && jni::get_env()->CallStaticBooleanMethod(s.bridge,s.healthy)==JNI_TRUE;
        bool failed=platform::jni_context::clear_exception();
        const bool blocked=!failed && s.shader_check && jni::get_env()->CallStaticBooleanMethod(s.bridge,s.shader_check)==JNI_TRUE;
        failed=platform::jni_context::clear_exception() || failed;
        enabled=alive && !blocked && !failed;
    }
    jni::get_env()->CallStaticVoidMethod(s.bridge,s.color_configure,static_cast<jint>(sky),static_cast<jint>(fog),
        static_cast<jfloat>(strength),match_fog ? JNI_TRUE : JNI_FALSE,enabled ? JNI_TRUE : JNI_FALSE);
    platform::jni_context::clear_exception();
    if (s.fog_bridge && s.fog_configure)
    {
        jni::get_env()->CallStaticVoidMethod(s.fog_bridge,s.fog_configure,static_cast<jint>(sky),static_cast<jint>(fog),
            static_cast<jfloat>(strength),match_fog ? JNI_TRUE : JNI_FALSE,enabled ? JNI_TRUE : JNI_FALSE);
        platform::jni_context::clear_exception();
    }
}
void sky_panorama::renderer::shutdown()
{
    auto& s=*state_; s.enabled=false;
    if (platform::jni_context::bind_current_thread() && s.bridge)
    {
        auto* env=jni::get_env();
        if (s.stop) env->CallStaticVoidMethod(s.bridge,s.stop);
        if (s.fog_bridge && s.fog_stop) env->CallStaticVoidMethod(s.fog_bridge,s.fog_stop);
        platform::jni_context::clear_exception();
        if (s.tooling)
        {
            s.tooling->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
            if (s.fog_changed && s.fog_class) s.tooling->RetransformClasses(1,&s.fog_class);
            s.tooling->DisposeEnvironment(); s.tooling=nullptr;
        }
        if (runtime::transforming.load(std::memory_order_acquire)==&s) runtime::transforming.store(nullptr,std::memory_order_release);
        if (s.fog_class) { env->DeleteGlobalRef(s.fog_class); s.fog_class=nullptr; }
        if (s.transformer) { env->DeleteGlobalRef(s.transformer); s.transformer=nullptr; }
        if (s.fog_bridge) { env->DeleteGlobalRef(s.fog_bridge); s.fog_bridge=nullptr; }
        s.fog_changed=false;
        env->UnregisterNatives(s.bridge); env->DeleteGlobalRef(s.bridge); s.bridge=nullptr;
    }
    if (runtime::current==&s) runtime::current=nullptr;
    if (s.owner && wglGetCurrentContext()==s.owner)
    {
        if (s.program) s.gl.DeleteProgram(s.program);
        if (s.vao) s.gl.DeleteVertexArrays(1,&s.vao);
        glDeleteTextures(static_cast<GLsizei>(s.textures.size()),s.textures.data());
    }
    s.owner=nullptr; s.program=s.vao=0; s.textures={}; s.last_time=s.phase=0;
}
const char* sky_panorama::renderer::error() const
{
    auto& s=*state_;
    if (s.bridge && s.shader_check && jni::get_env())
    {
        const bool blocked=jni::get_env()->CallStaticBooleanMethod(s.bridge,s.shader_check)==JNI_TRUE;
        if (!platform::jni_context::clear_exception() && blocked) return "Disable the Iris shader pack to use panoramas.";
        const bool alive=jni::get_env()->CallStaticBooleanMethod(s.bridge,s.healthy)==JNI_TRUE;
        if (!platform::jni_context::clear_exception() && !alive) return "The panorama bridge stopped; reload the client.";
    }
    return s.failure.c_str();
}
