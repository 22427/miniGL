#ifdef HGLES_USE_PI

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "hgles_window_pi.h"
#include "hgles_glad.h"
#include <iostream>


namespace hgles
{


bool create_window(EGL_DISPMANX_WINDOW_T& nativewindow,
				   int& width,
				   int& height,
				   unsigned int x,
				   unsigned int y)
{
	bcm_host_init();
	int32_t success = 0;


	DISPMANX_ELEMENT_HANDLE_T dispman_element;
	DISPMANX_DISPLAY_HANDLE_T dispman_display;
	DISPMANX_UPDATE_HANDLE_T dispman_update;
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;


			unsigned int display_width = 0;
			unsigned int display_height =0 ;

	success = graphics_get_display_size(
				0 /* LCD */,
				&display_width,
				&display_height);

	if ( success < 0 )
	{
		return false;
	}

	if(width<0)
	{
		width = display_width;
	}

	if(height <0)
	{
		height = display_height;
	}


	dst_rect.x = x;
	dst_rect.y = y;
	dst_rect.width = width;
	dst_rect.height = height;

	src_rect.x = x;
	src_rect.y = y;
	src_rect.width = width << 16;
	src_rect.height = height << 16;

	dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
	dispman_update = vc_dispmanx_update_start( 0 );

	dispman_element = vc_dispmanx_element_add(
				dispman_update,
				dispman_display,
				0/*layer*/,
				&dst_rect,
				0/*src*/,
				&src_rect,
				DISPMANX_PROTECTION_NONE,
				0 /*alpha*/,
				0/*clamp*/,
				(DISPMANX_TRANSFORM_T) 0/*transform*/);

	nativewindow.element = dispman_element;
	nativewindow.width = width;
	nativewindow.height = height;
	vc_dispmanx_update_submit_sync( dispman_update );

	return true;

}



bool create_egl_context(
		EGLNativeWindowType hWnd,
		EGLDisplay *eglDisplay,
		EGLContext *eglContext,
		EGLSurface *eglSurface,
		EGLint attribList[],
		EGLContext shared,
		bool,// pbuffered,
		int,// width,
		int)// height)
{
	EGLint numConfigs;
	EGLint majorVersion;
	EGLint minorVersion;
	EGLDisplay display;
	EGLContext context;
	EGLSurface surface;
	EGLConfig config;
	EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION,
								2,
								EGL_NONE };


	// Get Display
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	if ( display  == EGL_NO_DISPLAY )
	{
		ERROR("EGL: get display failed");
		return false;
	}

	// Initialize EGL
	if ( !eglInitialize(display, &majorVersion, &minorVersion) )
	{
		ERROR("EGL: initialization failed: %#x",eglGetError());
		return false;
	}

	// Get configs
	if ( !eglGetConfigs(display, NULL, 0, &numConfigs) )
	{
		ERROR("EGL: get config failed");
		return false;
	}

	// Choose config
	if ( !eglChooseConfig(display,
						  attribList,
						  &config,
						  1,
						  &numConfigs) )
	{
		ERROR("EGL: choose config failed!");
		return false;
	}
	// Create a surface
	surface = eglCreateWindowSurface(
				display,
				config,
				(EGLNativeWindowType)hWnd,
				NULL);

	if ( surface == EGL_NO_SURFACE )
	{
		ERROR("EGL: surface creation failed!");
		return false;
	}

	// Create a GL context
	context = eglCreateContext(display,
							   config,
							   shared,
							   contextAttribs );
	if ( context == EGL_NO_CONTEXT )
	{
		ERROR("EGL: context creation failed!");
		return false;
	}

	// Make the context current
	if ( !eglMakeCurrent(display, surface, surface, context) )
	{
		ERROR("EGL: make current failed!");
		return false;
	}

	*eglDisplay = display;
	*eglSurface = surface;
	*eglContext = context;
	return true;
}



static void* open_handle(const char *lib_names[], int length) {
	int i;
	void *handle;

	for (i = 0; i < length; ++i) {
#ifdef _WIN32
		handle = LoadLibraryA(lib_names[i]);
#else
		handle = dlopen(lib_names[i], RTLD_LAZY | RTLD_GLOBAL);
#endif
		if (handle != NULL) {
			return handle;
		}
	}

	return NULL;
}

static void close_handle(void* handle) {
	if (handle != NULL) {
#ifdef _WIN32
		FreeLibrary(handle);
#else
		dlclose(handle);
#endif
	}
}

static void* dlsym_handle(void* handle, const char *name) {
	if (handle == NULL) {
		return NULL;
	}

#ifdef _WIN32
	return GetProcAddress(handle, name);
#else
	return dlsym(handle, name);
#endif
}

static void* global_egl_handle;

static void* load_with_egl(const char *name)
{
	void* result = dlsym_handle(global_egl_handle, name);
	if (!result)
	{
		result = (void*) eglGetProcAddress(name);
	}
	return result;
}



Window::Window(const uint32_t w,
			   const uint32_t h,
			   const uint32_t x,
			   const uint32_t y)
{
	m_pos  = glm::ivec2(x,y);
	m_sze  = glm::ivec2(w,h);
	m_log = [](const std::string& str){std::cout<<"[ L ] "<<str<<"\n";};
	m_warning = [](const std::string& str){std::cout<<"[ W ] "<<str<<"\n";};
	m_error= [](const std::string& str){std::cerr<<"[ E ] "<<str<<"\n";};

	m_should_close = false;
	EGLint attribList[] =
	{
		EGL_RED_SIZE,      8,
		EGL_GREEN_SIZE,    8,
		EGL_BLUE_SIZE,     8,
		EGL_ALPHA_SIZE,    8,
		EGL_DEPTH_SIZE,   24,
		EGL_STENCIL_SIZE,  0,
		EGL_SAMPLE_BUFFERS,1,
		EGL_NONE
	};


	if(!create_window(m_win_handle,m_sze.x,m_sze.y,x,y))
	{
		m_error("failed creating 'window'!");
		exit(0);
	}


	if(!create_egl_context(reinterpret_cast<EGLNativeWindowType>(&m_win_handle),
						   &m_display,
						   &m_context,
						   &m_surface,
						   attribList,
						   EGL_NO_CONTEXT,
						   false,w,h))
	{
		m_error("failed creating context!");
		exit(0);
	}
	make_current();
#ifdef __APPLE__
	static const char *NAMES[] = {"todo.dylib"};
#elif defined _WIN32
	static const char *NAMES[] = {"todo.dll"};
#else
	static const char *NAMES[] = {"libbrcmGLESv2.so","libGLESv2.so"};
#endif	
	global_egl_handle = open_handle(NAMES, sizeof(NAMES) / sizeof(NAMES[0]));
	if (global_egl_handle) {
		if(!gladLoadGLES2Loader((GLADloadproc) load_with_egl))
		{
			m_error("Failed to load GL using GLAD!");
			exit(0);
		}

		close_handle(global_egl_handle);
	}
}











Window::~Window(){}

void Window::make_current()
{
	if(!eglMakeCurrent(m_display, m_surface, m_surface, m_context))
		m_error("eglMakeCurrent failed!");
}

void Window::unmake_current()
{
	if(!eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
		m_error("eglMakeCurrent failed!");
}

void Window::swap_buffers()
{
	eglSwapBuffers(m_display,m_surface);
}

void Window::poll_events(){}

void Window::set_title(const std::string &){}

void Window::toggle_fullscreen(){/*TODO*/}

void Window::set_fullscreen(bool){/*TODO*/}

void Window::toggle_decoration(){}

void Window::set_decoration(bool){}

bool Window::should_close(){return m_should_close;}

void Window::set_should_close(bool s_c){m_should_close =s_c;}

void Window::add_window_listener(WindowListener *l)
{
	m_window_listeners.insert(l);
	l->size_changed(m_sze.x,m_sze.y);
	l->position_changed(m_pos.x,m_pos.y);
}

void Window::remove_window_listener(WindowListener * l)
{
	m_window_listeners.erase(l);
}

void Window::set_size(const int , const int ){/*TODO*/}

void Window::set_size(const glm::ivec2 &sze){set_size(sze.x,sze.y);}

glm::ivec2 Window::get_size() const{return m_sze;}

void Window::set_position(const int , const int ){}

void Window::set_position(const glm::ivec2 &pos){set_position(pos.x,pos.y);}

glm::ivec2 Window::get_position() const {return m_pos;}
}

#endif
