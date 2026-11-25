// Template, IGAD version 3
// IGAD/NHTV/UU - Jacco Bikker - 2006-2022

#define GLAD_GL_IMPLEMENTATION
#include "precomp.h"

#ifdef _MSC_VER
#pragma comment( linker, "/subsystem:windows /ENTRY:mainCRTStartup" )
#endif

using namespace Tmpl8;

// Enable usage of dedicated GPUs in notebooks
// Note: this does cause the linker to produce a .lib and .exp file;
// see http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
#ifdef WIN32
extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}

extern "C"
{
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

GLFWwindow* window = 0;
static bool hasFocus = true, running = true;
static GLTexture* renderTarget = 0;
static int scrwidth = 0, scrheight = 0;
static TheApp* app = 0;
bool IGP_detected = false;
uint keystate[256] = { 0 };

// static member data for instruction set support class
static const CPUCaps cpucaps;

// provide access to the render target, for OpenCL / OpenGL interop
GLTexture* GetRenderTarget() { return renderTarget; }

// provide access to window focus state
bool WindowHasFocus() { return hasFocus; }

// provide access to key state array
bool IsKeyDown( const uint key ) { return keystate[key & 255] == 1; }

// GLFW callbacks
void InitRenderTarget( int w, int h )
{
	// allocate render target and surface
	scrwidth = w, scrheight = h;
	renderTarget = new GLTexture( scrwidth, scrheight, GLTexture::INTTARGET );
}
void ReshapeWindowCallback( GLFWwindow*, int w, int h )
{
	glViewport( 0, 0, w, h );
}
void KeyEventCallback( GLFWwindow*, int key, int, int action, int )
{
	if (key == GLFW_KEY_ESCAPE) running = false;
	if (action == GLFW_PRESS) { if (app) if (key >= 0) app->KeyDown( key ); keystate[key & 255] = 1; }
	else if (action == GLFW_RELEASE) { if (app) if (key >= 0) app->KeyUp( key ); keystate[key & 255] = 0; }
}
void CharEventCallback( GLFWwindow*, uint ) { /* nothing here yet */ }
void WindowFocusCallback( GLFWwindow*, int focused ) { hasFocus = (focused == GL_TRUE); }
void MouseButtonCallback( GLFWwindow*, int button, int action, int )
{
	if (action == GLFW_PRESS) { if (app) app->MouseDown( button ); }
	else if (action == GLFW_RELEASE) { if (app) app->MouseUp( button ); }
}
void MouseScrollCallback( GLFWwindow*, double, double y )
{
	app->MouseWheel( (float)y );
}
void MousePosCallback( GLFWwindow*, double x, double y )
{
	if (app) app->MouseMove( (int)x, (int)y );
}
void ErrorCallback( int, const char* description )
{
	fprintf( stderr, "GLFW Error: %s\n", description );
}

// Application entry point
int main()
{
	// open a window
	if (!glfwInit()) FatalError( "glfwInit failed." );
	glfwSetErrorCallback( ErrorCallback );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 ); // 3.3 is enough for our needs
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
	glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
	glfwWindowHint( GLFW_STENCIL_BITS, GL_FALSE );
	glfwWindowHint( GLFW_RESIZABLE, GL_FALSE /* easier :) */ );
#ifdef FULLSCREEN
	window = glfwCreateWindow( SCRWIDTH, SCRHEIGHT, "Tmpl8RT", glfwGetPrimaryMonitor(), 0 );
#else
#ifdef DOUBLESIZE
	window = glfwCreateWindow( SCRWIDTH * 2, SCRHEIGHT * 2, "Tmpl8RT", 0, 0 );
#else
	window = glfwCreateWindow( SCRWIDTH, SCRHEIGHT, "Tmpl8RT", 0, 0 );
#endif
#endif
	if (!window) FatalError( "glfwCreateWindow failed." );
	glfwMakeContextCurrent( window );
	// register callbacks
	glfwSetWindowSizeCallback( window, ReshapeWindowCallback );
	glfwSetKeyCallback( window, KeyEventCallback );
	glfwSetWindowFocusCallback( window, WindowFocusCallback );
	glfwSetMouseButtonCallback( window, MouseButtonCallback );
	glfwSetScrollCallback( window, MouseScrollCallback );
	glfwSetCursorPosCallback( window, MousePosCallback );
	glfwSetCharCallback( window, CharEventCallback );
	// initialize GLAD
	if (!gladLoadGL( (GLADloadfunc)glfwGetProcAddress )) FatalError( "gladLoadGL failed." );
	glfwSwapInterval( 0 );
	// prepare OpenGL state
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_BLEND );
	CheckGL();
	char* vendor = (char*)glGetString( GL_VENDOR );
	char* renderer = (char*)glGetString( GL_RENDERER );
	if (strstr( vendor, "Intel" )) IGP_detected = true;
	if (strstr( renderer, "Arc" )) IGP_detected = false;
	// we want a console window for text output
#ifndef FULLSCREEN
	CONSOLE_SCREEN_BUFFER_INFO coninfo;
	AllocConsole();
	GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &coninfo );
	coninfo.dwSize.X = 1280;
	coninfo.dwSize.Y = 800;
	SetConsoleScreenBufferSize( GetStdHandle( STD_OUTPUT_HANDLE ), coninfo.dwSize );
	FILE* file = nullptr;
	freopen_s( &file, "CON", "w", stdout );
	freopen_s( &file, "CON", "w", stderr );
	SetWindowPos( GetConsoleWindow(), HWND_TOP, 0, 0, 1280, 800, 0 );
	glfwShowWindow( window );
#endif
	// initialize application
	InitRenderTarget( SCRWIDTH, SCRHEIGHT );
	Surface* screen = new Surface( SCRWIDTH, SCRHEIGHT );
	app = new Renderer();
#if 0
	// deserizalize
	FILE* f = fopen( "appstate.dat", "rb" );
	if (f)
	{
		uint version, appSize;
		fread( &version, 1, 4, f );
		fread( &appSize, 1, 4, f );
		// we check file format version and sizeof( Renderer ) so we don't accidentally read
		// an app state that is invalid.
		uint base_size = (uint)(((uchar*)&app->end_of_base_class + 4) - (uchar*)app);
		uint rendererSize = sizeof( Renderer ) - base_size;
		if (version == 0x10001006 && appSize == rendererSize)
		{
			// we only overwrite the data unique to the derived Renderer class -
			// disturbing other data is not consistent between debug and release.
			void* target = ((uchar*)app + sizeof( Renderer )) - rendererSize;
			fread( target, 1, rendererSize, f );
		}
		fclose( f );
	}
	// reconstruct scene, so we can change it and still de-serialize
	((Renderer*)app)->scene = Scene();
#endif
	// finalize app
	app->screen = screen;
	app->Init();
	// prep imgui
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL( window, true );
	ImGui_ImplOpenGL3_Init( 0 );
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = "./imgui.ini";
	// done, enter main loop
#if 1
	// basic shader: apply gamma correction
	Shader* shader = new Shader(
		"#version 330\nin vec4 p;\nin vec2 t;out vec2 u;void main(){u=t;gl_Position=p;}",
		"#version 330\nuniform sampler2D c;in vec2 u;out vec4 f;void main(){f=/*sqrt*/(texture(c,u));}", true );
#else
	// fxaa shader
	Shader* shader = new Shader(
		"#version 330\nin vec4 p;\nin vec2 t;out vec2 uv;void main(){uv=t;gl_Position=p;}",
		// FXAA 3.11 Implementation - effendiian
		// https://www.shadertoy.com/view/ttXGzn
		"#version 330\nuniform sampler2D tex;\nin vec2 uv;out vec4 f; \n"							\
		"#define FXAA_LUMINANCE 			1					\n"									\
		"#define FXAA_EDGE_THRESHOLD	  	(1.0/8.0)			\n"									\
		"#define FXAA_EDGE_THRESHOLD_MIN  	(1.0/24.0)			\n"									\
		"#define FXAA_SEARCH_STEPS			32					\n"									\
		"#define FXAA_SEARCH_ACCELERATION 	1					\n"									\
		"#define FXAA_SEARCH_THRESHOLD		(1.0/4.0)			\n"									\
		"#define FXAA_SUBPIX				2 // 1 is crisper	\n"									\
		"#define FXAA_SUBPIX_CAP			(3.0/4.0)			\n"									\
		"#define FXAA_SUBPIX_TRIM			(1.0/4.0)			\n"									\
		"#define FXAA_SUBPIX_TRIM_SCALE (1.0/(1.0 - FXAA_SUBPIX_TRIM))	\n"							\
		"float lum( vec3 color ) {\n #if FXAA_LUMINANCE == 0 \n"									\
		"	return color.x * 0.2126729 + color.y * 0.7151522 + color.z * 0.0721750; \n"				\
		"#else \n return color.g * (0.587 / 0.299) + color.r; \n #endif \n }"						\
		"float vertEdge( float lumaO, float lumaN, float lumaE, float lumaS,"						\
		"	float lumaW, float lumaNW, float lumaNE, float lumaSW, float lumaSE )"					\
		"{	float top = (0.25 * lumaNW) + (-0.5 * lumaN) + (0.25 * lumaNE);"						\
		"	float middle = (0.50 * lumaW) + (-1.0 * lumaO) + (0.50 * lumaE);"						\
		"	float bottom = (0.25 * lumaSW) + (-0.5 * lumaS) + (0.25 * lumaSE);"						\
		"	return abs( top ) + abs( middle ) + abs( bottom ); }"									\
		"float horEdge( float lumaO, float lumaN, float lumaE, float lumaS,"						\
		"	float lumaW, float lumaNW, float lumaNE, float lumaSW, float lumaSE )"					\
		"{	float top = (0.25 * lumaNW) + (-0.5 * lumaW) + (0.25 * lumaSW);"						\
		"	float middle = (0.50 * lumaN) + (-1.0 * lumaO) + (0.50 * lumaS);"						\
		"	float bottom = (0.25 * lumaNE) + (-0.5 * lumaE) + (0.25 * lumaSE);"						\
		"	return abs( top ) + abs( middle ) + abs( bottom ); }"									\
		"vec3 fxaa( vec2 textureDimensions, vec2 uv )"												\
		"{	vec2 texel = vec2( 1.0, 1.0 ) / textureDimensions;"										\
		"	vec3 rgbN = texture( tex, uv + vec2( 0, -texel.y ) ).rgb,"								\
		"		 rgbW = texture( tex, uv + vec2( -texel.x, 0 ) ).rgb,"								\
		"		 rgbO = texture( tex, uv + vec2( 0, 0 ) ).rgb,"										\
		"		 rgbE = texture( tex, uv + vec2( texel.x, 0 ) ).rgb,"								\
		"		 rgbS = texture( tex, uv + vec2( 0, texel.y ) ).rgb;"								\
		"	float lumaN = lum( rgbN ), lumaW = lum( rgbW );"										\
		"	float lumaO = lum( rgbO ), lumaE = lum( rgbE ), lumaS = lum( rgbS );"					\
		"	float minLuma = min( lumaO, min( min( lumaN, lumaW ), min( lumaS, lumaE ) ) );"			\
		"	float maxLuma = max( lumaO, max( max( lumaN, lumaW ), max( lumaS, lumaE ) ) );"			\
		"	float localContrast = maxLuma - minLuma;"												\
		"	if (localContrast < max( FXAA_EDGE_THRESHOLD_MIN, maxLuma* FXAA_EDGE_THRESHOLD ))"		\
		"		return rgbO;"																		\
		"	vec3 rgbL = rgbN + rgbW + rgbO + rgbE + rgbS;"											\
		"	float lumaL = (lumaN + lumaW + lumaS + lumaE) * 0.25;"									\
		"	float pixelContrast = abs( lumaL - lumaO );"											\
		"	float contrastRatio = pixelContrast / localContrast;"									\
		"	float lowpassBlend = 0;			\n"														\
		"#if FXAA_SUBPIX == 1				\n"														\
		"	lowpassBlend = max( 0.0, contrastRatio - FXAA_SUBPIX_TRIM ) * FXAA_SUBPIX_TRIM_SCALE;"	\
		"	lowpassBlend = min( FXAA_SUBPIX_CAP, lowpassBlend );	\n"								\
		"#elif FXAA_SUBPIX == 2				\n"														\
		"	lowpassBlend = contrastRatio;	\n"														\
		"#endif								\n"														\
		"	vec3 rgbNW = texture( tex, uv + vec2( -texel.x, -texel.y ) ).rgb,"						\
		"		 rgbNE = texture( tex, uv + vec2( texel.x, -texel.y ) ).rgb,"						\
		"		 rgbSW = texture( tex, uv + vec2( -texel.x, texel.y ) ).rgb,"						\
		"		 rgbSE = texture( tex, uv + vec2( texel.x, texel.y ) ).rgb;"						\
		"	rgbL += (rgbNW + rgbNE + rgbSW + rgbSE);"												\
		"	rgbL *= (1.0 / 9.0);"																	\
		"	float lumaNW = lum( rgbNW ), lumaNE = lum( rgbNE );"									\
		"	float lumaSW = lum( rgbSW ), lumaSE = lum( rgbSE );"									\
		"	float edgeVert = vertEdge( lumaO, lumaN, lumaE, lumaS, lumaW, lumaNW, lumaNE, lumaSW, lumaSE );" \
		"	float edgeHori = horEdge( lumaO, lumaN, lumaE, lumaS, lumaW, lumaNW, lumaNE, lumaSW, lumaSE );" \
		"	bool isHorizontal = edgeHori >= edgeVert;"												\
		"	float edgeSign = isHorizontal ? -texel.y : -texel.x;"									\
		"	float gradNeg = isHorizontal ? abs( lumaN - lumaO ) : abs( lumaW - lumaO );"			\
		"	float gradPos = isHorizontal ? abs( lumaS - lumaO ) : abs( lumaE - lumaO );"			\
		"	float lumaNeg = isHorizontal ? ((lumaN + lumaO) * 0.5) : ((lumaW + lumaO) * 0.5);"		\
		"	float lumaPos = isHorizontal ? ((lumaS + lumaO) * 0.5) : ((lumaE + lumaO) * 0.5);"		\
		"	bool isNegative = (gradNeg >= gradPos);"												\
		"	float gradientHighest = isNegative ? gradNeg : gradPos;"								\
		"	float lumaHighest = isNegative ? lumaNeg : lumaPos;"									\
		"	if (isNegative) edgeSign *= -1.0;"														\
		"	vec2 pointN = vec2( 0.0, 0.0 );"														\
		"	pointN.x = uv.x + (isHorizontal ? 0.0 : edgeSign * 0.5);"								\
		"	pointN.y = uv.y + (isHorizontal ? edgeSign * 0.5 : 0.0);"								\
		"	gradientHighest *= FXAA_SEARCH_THRESHOLD;"												\
		"	vec2 pointP = pointN;"																	\
		"	vec2 offset = isHorizontal ? vec2( texel.x, 0.0 ) : vec2( 0.0, texel.y );"				\
		"	float lumaNegEnd = lumaNeg, lumaPosEnd = lumaPos;"										\
		"	bool searchNeg = false, searchPos = false;\n"											\
		"#if FXAA_SEARCH_ACCELERATION == 1\n"														\
		"	pointN -= offset, pointP += offset;\n"													\
		"#elif FXAA_SEARCH_ACCELERATION == 2\n"														\
		"	pointN -= offset * 1.5, pointP += offset * 1.5, offset *= 2;\n"							\
		"#elif FXAA_SEARCH_ACCELERATION == 3\n"														\
		"	pointN -= offset * 2, pointP += offset * 2, offset *= 3;\n"								\
		"#elif FXAA_SEARCH_ACCELERATION == 4\n"														\
		"	pointN -= offset * 2.5, pointP += offset * 2.5, offset *= 4;\n"							\
		"#endif\n"																					\
		"	for (int i = 0; i < FXAA_SEARCH_STEPS; i++) {\n"										\
		"	#if FXAA_SEARCH_ACCELERATION == 1\n"													\
		"		if (!searchNeg) lumaNegEnd = lum( texture( tex, pointN ).rgb );"					\
		"		if (!searchPos) lumaPosEnd = lum( texture( tex, pointP ).rgb );\n"					\
		"	#else \n"																				\
		"		if (!searchNeg) lumaNegEnd = lum( textureGrad( tex, pointN, offset, offset ).rgb );"	\
		"		if (!searchPos) lumaPosEnd = lum( textureGrad( tex, pointP, offset, offset ).rgb );\n"	\
		"	#endif \n"																				\
		"		searchNeg = searchNeg || (abs( lumaNegEnd - lumaHighest ) >= gradientHighest);"		\
		"		searchPos = searchPos || (abs( lumaPosEnd - lumaPos ) >= gradPos);"					\
		"		if (searchNeg && searchPos) break;"													\
		"		if (!searchNeg) pointN -= offset;"													\
		"		if (!searchPos) pointP += offset; }"												\
		"	float distanceNeg = isHorizontal ? uv.x - pointN.x : uv.y - pointN.y;"					\
		"	float distancePos = isHorizontal ? pointP.x - uv.x : pointP.y - uv.y;"					\
		"	bool isCloserToNegative = distanceNeg < distancePos;"									\
		"	float lumaEnd = isCloserToNegative ? lumaNegEnd : lumaPosEnd;"							\
		"	if (((lumaO - lumaNeg) < 0.0) == ((lumaEnd - lumaNeg) < 0.0)) edgeSign = 0.0;"			\
		"	float spanLen = distancePos + distanceNeg;"												\
		"	float dist = isCloserToNegative ? distanceNeg : distancePos;"							\
		"	float subOffs = (0.5 + (dist * (-1.0 / spanLen))) * edgeSign;"							\
		"	vec3 rgbOffset = textureLod( tex, vec2( uv.x + (isHorizontal ? 0.0 :"					\
		"		subOffs), uv.y + (isHorizontal ? subOffs : 0.0) ), 0.0 ).rgb;"						\
		"	return mix( rgbOffset, rgbL, lowpassBlend ); }"											\
		"void main(){f=vec4(sqrt(fxaa(vec2(1240,800),uv)),1);}", true );
#endif
	float deltaTime = 0;
	static int frameNr = 0;
	static Timer timer;
	while (!glfwWindowShouldClose( window ))
	{
		deltaTime = min( 500.0f, 1000.0f * timer.elapsed() );
		timer.reset();
		app->Tick( deltaTime );
		// send the rendering result to the screen using OpenGL
		if (frameNr++ > 1)
		{
			// draw template application output
			if (app->screen) renderTarget->CopyFrom( app->screen );
			shader->Bind();
			shader->SetInputTexture( 0, "c", renderTarget );
			DrawQuad();
			shader->Unbind();
			// update imgui
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			app->uiUpdated = true;
			app->UI(); // app->uiUpdated will be false if Render::UI() was not implemented
			if (app->uiUpdated)
			{
				ImGui::Render();
				ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
				int display_w, display_h;
				glfwGetFramebufferSize( window, &display_w, &display_h );
				glViewport( 0, 0, display_w, display_h );
			}
			// finalize frame
			glfwSwapBuffers( window );
			glfwPollEvents();
		}
		if (!running) break;
	}
#if 0
	// serialize
	f = fopen( "appstate.dat", "wb" );
	uint base_size = (uint)(((uchar*)&app->end_of_base_class + 4) - (uchar*)app);
	uint rendererSize = sizeof( Renderer ) - base_size;
	uint version = 0x10001006;
	fwrite( &version, 1, 4, f );
	fwrite( &rendererSize, 1, 4, f );
	void* source = ((uchar*)app + sizeof( Renderer )) - rendererSize;
	fwrite( source, 1, rendererSize, f );
	fclose( f );
#endif
	// close down
	app->Shutdown();
	Kernel::KillCL();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow( window );
	glfwTerminate();
	return 0;
}

// Jobmanager implementation
DWORD JobThreadProc( LPVOID lpParameter )
{
	JobThread* JobThreadInstance = (JobThread*)lpParameter;
	JobThreadInstance->BackgroundTask();
	return 0;
}

void JobThread::CreateAndStartThread( unsigned int threadId )
{
	m_GoSignal = CreateEvent( 0, FALSE, FALSE, 0 );
	m_ThreadHandle = CreateThread( 0, 0, (LPTHREAD_START_ROUTINE)&JobThreadProc, (LPVOID)this, 0, 0 );
	m_ThreadID = threadId;
}
void JobThread::BackgroundTask()
{
	while (1)
	{
		WaitForSingleObject( m_GoSignal, INFINITE );
		while (1)
		{
			Job* job = JobManager::GetJobManager()->GetNextJob();
			if (!job)
			{
				JobManager::GetJobManager()->ThreadDone( m_ThreadID );
				break;
			}
			job->RunCodeWrapper();
		}
	}
}

void JobThread::Go()
{
	SetEvent( m_GoSignal );
}

void Job::RunCodeWrapper()
{
	Main();
}

JobManager* JobManager::m_JobManager = 0;

JobManager::JobManager( unsigned int threads ) : m_NumThreads( threads )
{
	InitializeCriticalSection( &m_CS );
}

JobManager::~JobManager()
{
	DeleteCriticalSection( &m_CS );
}

void JobManager::CreateJobManager( unsigned int numThreads )
{
	m_JobManager = new JobManager( numThreads );
	m_JobManager->m_JobThreadList = new JobThread[numThreads];
	for (unsigned int i = 0; i < numThreads; i++)
	{
		m_JobManager->m_JobThreadList[i].CreateAndStartThread( i );
		m_JobManager->m_ThreadDone[i] = CreateEvent( 0, FALSE, FALSE, 0 );
	}
	m_JobManager->m_JobCount = 0;
}

void JobManager::AddJob2( Job* a_Job )
{
	m_JobList[m_JobCount++] = a_Job;
}

Job* JobManager::GetNextJob()
{
	Job* job = 0;
	EnterCriticalSection( &m_CS );
	if (m_JobCount > 0) job = m_JobList[--m_JobCount];
	LeaveCriticalSection( &m_CS );
	return job;
}

void JobManager::RunJobs( bool MT )
{
	if (m_JobCount == 0) return;
	if (MT)
	{
		for (unsigned int i = 0; i < m_NumThreads; i++) m_JobThreadList[i].Go();
		WaitForMultipleObjects( m_NumThreads, m_ThreadDone, TRUE, INFINITE );
	}
	else
	{
		// run the jobs on a single core, for debugging
		for (uint i = 0; i < m_JobCount; i++) m_JobList[i]->Main();
	}
}

void JobManager::ThreadDone( unsigned int n )
{
	SetEvent( m_ThreadDone[n] );
}

DWORD CountSetBits( ULONG_PTR bitMask )
{
	DWORD LSHIFT = sizeof( ULONG_PTR ) * 8 - 1, bitSetCount = 0;
	ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;
	for (DWORD i = 0; i <= LSHIFT; ++i) bitSetCount += ((bitMask & bitTest) ? 1 : 0), bitTest /= 2;
	return bitSetCount;
}

void JobManager::GetProcessorCount( uint& cores, uint& logical )
{
	// https://github.com/GPUOpen-LibrariesAndSDKs/cpu-core-counts
	cores = logical = 0;
	char* buffer = NULL;
	DWORD len = 0;
	if (FALSE == GetLogicalProcessorInformationEx( RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer, &len ))
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			buffer = (char*)malloc( len );
			if (GetLogicalProcessorInformationEx( RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer, &len ))
			{
				DWORD offset = 0;
				char* ptr = buffer;
				while (ptr < buffer + len)
				{
					PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pi = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
					if (pi->Relationship == RelationProcessorCore)
					{
						cores++;
						for (size_t g = 0; g < pi->Processor.GroupCount; ++g)
							logical += CountSetBits( pi->Processor.GroupMask[g].Mask );
					}
					ptr += pi->Size;
				}
			}
			free( buffer );
		}
	}
}

JobManager* JobManager::GetJobManager()
{
	if (!m_JobManager)
	{
		uint c, l;
		GetProcessorCount( c, l );
		CreateJobManager( l );
	}
	return m_JobManager;
}

// Helper functions
bool FileIsNewer( const char* file1, const char* file2 )
{
	struct stat f1;
	struct stat f2;

	auto ret = stat( file1, &f1 );
	FATALERROR_IF( ret, "File %s not found!", file1 );

	if (stat( file2, &f2 )) return true; // second file does not exist

#ifdef _MSC_VER
	return f1.st_mtime >= f2.st_mtime;
#else
	if (f1.st_mtim.tv_sec >= f2.st_mtim.tv_sec)
		return true;
	return f1.st_mtim.tv_nsec >= f2.st_mtim.tv_nsec;
#endif
}

bool FileExists( const char* f )
{
	ifstream s( f );
	return s.good();
}

bool RemoveFile( const char* f )
{
	if (!FileExists( f )) return false;
	return !remove( f );
}

uint FileSize( string filename )
{
	ifstream s( filename );
	return s.good();
}

string TextFileRead( const char* _File )
{
	ifstream s( _File );
	string str( (istreambuf_iterator<char>( s )), istreambuf_iterator<char>() );
	s.close();
	return str;
}

int LineCount( const string s )
{
	const char* p = s.c_str();
	int lines = 0;
	while (*p) if (*p++ == '\n') lines++;
	return lines;
}

void TextFileWrite( const string& text, const char* _File )
{
	ofstream s( _File, ios::binary );
	int len = (int)text.size();
	s.write( (const char*)&len, sizeof( len ) );
	s.write( text.c_str(), len );
}

// EOF
